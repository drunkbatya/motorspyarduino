 // MotorSpy firmware v  1.3
// Written by DrunkBatya
#include "MotorSpyArduino.h"
#include "modbus.c"

void (* softwareReset) (void) = 0;
Data data;
Settings settings;
uint8_t mbuf[MBUFSIZE];

float 		time;				// время
uint16_t 	intCount	=	0;	// счётчик прерываний
bool 		doint		= 	true;	// флаг разрешения реакции на прерывание
bool 		doloop		=	true;	// флаг разрешения исполнения функции замера в основном цикле
uint16_t 	pos 		= 	1;	// указатель на позицию в массиве, начинаем с 1, что бы при умножении не было ошибок
bool		ready		=	false;

void setup() {
	//EEPROM READ
	memset(&settings, 0, sizeof(settings));
	for (int i = 0; i<(EEPROM_END - EEPROM_START); i++) {
		uint8_t v = EEPROM.read(i);
		memcpy((uint8_t *)((uint32_t)(&settings) + EEPROM_START + i), &v, sizeof(uint8_t));
	}
	//EEPROM READ

	//TEMPORARY EEPROM SETTINGS
	data.divider = 1; // делитель в рассчёте частоты дискретизации
	data.broke = 0; // колличество потеряных сессий
	settings.mbus_id = 0x11;
	settings.state = 0x1234;
	settings.mbus_baud = 57600;
	settings.mbus_timeout = 5;
	settings.wait_trh = 2000;
	settings.mbus_delay_a = 14000.0f;
	settings.mbus_delay_b = 0.0f;
	//TEMPORARY EEPROM SETTINGS

	Serial.setTimeout(settings.mbus_timeout);
	Serial.begin(settings.mbus_baud);
	pinMode(PIN_INT, INPUT_PULLUP); // нога D2 как вход, подтянутый к плюсу
	pinMode(PIN_INT2, INPUT_PULLUP);
	pinMode(PIN_485, OUTPUT);
	digitalWrite(PIN_485, LOW);
	attachInterrupt(digitalPinToInterrupt(2), int1, RISING); // нога 2 для сигнала прерывания
	wdt_enable(WDTO_4S);

	// DEBUG
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);
	// DEBUG
}

void int1() {
	intCount++; // инкрементим счётчик прерываний при каждом импульсе
	if (intCount < 2) { // это нужно для того, что-бы при включении Arduino не срабатывало сразу прерывание
		return;
	}
	time = millis(); // запоминаем время прихода прерывания
	if (!doint) { // если стоит флаг запрета рекации на прерывание
		return; // то выходим из функции прерывания
	}
	else if ((intCount % data.divider) == 0) { // частота дискритезации
		doloop = true; // флаг для начала измерения
	}
	return;
}

void zip () {
	uint16_t i;
	for (i=1; i<ARRSIZE; i++) { // перебираем ячейки
		data.Array[i] = ((data.Array[2*i] + data.Array[(2*i)-1]) / 2); // находим среднее арифмитическое каждой пары ячеек и записываем
	}
	pos = ARRSIZE / 2; // возвращаем указатель позиции на середину массива
	data.divider = data.divider * 2; // уменьшаем частоту дискретизации
	clear(pos); // очищаем то, куда будем писать (вторая половина массива)
	return;
}
void clear (uint16_t i) { // очистка от заданого значения до конца массива
	for (i; i<ARRSIZE; i++) {
		data.Array[i] = 0; // перебираем все указатели и заполняем ячейки нулями
	}
	if (i == 1) {
		pos = 1; // установка позиции в начало
		data.divider = 1; // дискретизация снова максимальная
		intCount = 1; // сбрасываем счётчик прерываний
		time = 0; // сбрасываем время ожидания
		ready = false;
	}
	return;
}

void calc () {
	uint16_t volt = 1; // данные замера
	uint16_t oldvolt = 0; // данные предыдущего замера
	doint = false; // запрещаем реакцию на прерывание
	while (volt > oldvolt) { // пока новое значение больше старого
		oldvolt = volt;
		volt = analogRead(0); // читаем значение со входа
	}
	data.Array[pos] = oldvolt; // записываем максимальное значение в массив
	oldvolt = 0; // обнуляем старое значение
	pos++; // инкрементим позицию
	if (pos >= ARRSIZE-1) { // если массив заполнен, начинаем сжимать его
		zip();
	}
	doint = true; // разрешаем последующую реакцию на прерывание
	return;
}

void mBus() {
	int serialBytes = Serial.readBytes(mbuf, MBUFSIZE);
	if(!checkDU(mbuf, serialBytes)) {
		return;
	}
	if (mbuf[0] != settings.mbus_id) {
		return;
	}
	uint8_t fn = mbuf[1];

	// READ HOLDING REGISTERS (SETTINGS)
	if (fn == 0x03) {
		swapByteOrder(mbuf, 2, 4);
		uint16_t A, Q;
		memcpy(&A, &mbuf[2], sizeof(uint16_t)); // адрес первого регистра
		memcpy(&Q, &mbuf[4], sizeof(uint16_t)); // колличество регистров, которые хотим считать
		// составляем пакет ответа
		mbuf[0] = settings.mbus_id;
		if ((A * 2 + 2 * Q) > sizeof(Settings) || (Q > 123) || (Q == 0) || (2 * Q + 9 > MBUFSIZE)) {
			serialBytes = 5;
			mbuf[1] = fn | 0x80; // при ошибке возращаем номер фукции с логическим или с 0x80
			mbuf[2] = 0x02; // код ошибки ILLEGAL DATA ADRESS
		}
		else {
			serialBytes = 5 + 2 * Q; // [id][fn][number of bytes][settings][CC]
			mbuf[1] = fn;
			//mbuf[2] = 7; // колличество байт, которые возвращаем
			mbuf[2] = 2 * Q; // колличество байт, которые возвращаем
			memcpy (&mbuf[3], (uint8_t *)((uint32_t)(&settings) + A * 2), 2 * Q);
			swapByteOrder(mbuf, 3, 2 * Q);
		}
		signDU(mbuf, serialBytes);
		mBusWrite(mbuf, serialBytes);
	}
	// --END-- READ HOLDING REGISTERS (SETTINGS) --END--

	// READ INPUT REGISTERS (DATA)
	else if (fn == 0x04) {
		swapByteOrder(mbuf, 2, 4);
		uint16_t A, Q;
		memcpy(&A, &mbuf[2], sizeof(uint16_t)); // адрес первого регистра
		memcpy(&Q, &mbuf[4], sizeof(uint16_t)); // колличество регистров, которые хотим считать
		// составляем пакет ответа
		mbuf[0] = settings.mbus_id;
		if ((A * 2 + 2 * Q) > sizeof(Data) || (Q > 123) || (Q == 0) || (2 * Q + 9 > MBUFSIZE)) {
			serialBytes = 5;
			mbuf[1] = fn | 0x80; // при ошибке возращаем номер фукции с логическим или с 0x80
			mbuf[2] = 0x02; // код ошибки ILLEGAL DATA ADRESS
		}
		else if (!ready) { // если устройство не готово отдать показания (процесс измерения идёт)
			serialBytes = 6;
			mbuf[1] = fn | 0x80;
			mbuf[2] = 0x04; // код ошибки UNKNOWN ERROR
			mbuf[3] = 0x01; // расширение кода ошибки - DEVICE ISN'T READY (NOT STANDART)
		}
		else {
			serialBytes = 5 + 2 * Q; // [id][fn][number of bytes][settings][CC]
			mbuf[1] = fn;
			mbuf[2] = 2 * Q; // колличество байт, которые возвращаем
			memcpy (&mbuf[3], (uint8_t *)((uint32_t)(&data) + A * 2), 2 * Q);
			swapByteOrder(mbuf, 3, 2 * Q);
		}
		signDU(mbuf, serialBytes);
		mBusWrite(mbuf, serialBytes);
	}
	// --END-- READ INPUT REGISTERS (DATA) --END--

	// WRITE MULTYPLY REGISTERS (SETTINGS)
	else if (fn == 0x10) {
		swapByteOrder(mbuf, 2, 4);
		uint16_t A, Q;
		uint8_t N;
		memcpy(&A, &mbuf[2], sizeof(uint16_t));
		memcpy(&Q, &mbuf[4], sizeof(uint16_t));
		memcpy(&N, &mbuf[6], sizeof(uint8_t));
		// составляем пакет ответа
		mbuf[0] = settings.mbus_id;
		if ((A * 2 + 2 * Q) > sizeof(Settings) || (Q > 123) || (2 * Q + 9 > MBUFSIZE) || (Q == 0) || (N != 2 * Q) || (N < serialBytes - 9)) {
			serialBytes = 5;
			mbuf[1] = fn | 0x80; // при ошибке возращаем номер фукции с логическим или с 0x80
			if (N != 2 * Q) {
				mbuf[2] = 0x03; // код ошибки ILLEGAL DATA VALUE
			}
			else {
				mbuf[2] = 0x02; // код ошибки ILLEGAL DATA ADRESS
			}
		}
		else {
			serialBytes = 8;
			mbuf[1] = fn;
			swapByteOrder(mbuf, 7, N);
			memcpy ((uint8_t *)((uint32_t)(&settings) + A * 2), &mbuf[7], N);
			swapByteOrder(mbuf, 2, 4);
		}
		signDU(mbuf, serialBytes);
		mBusWrite(mbuf, serialBytes);
	}
	// --END-- WRITE MULTYPLY REGISTERS (DATA) --END--

	// USER FUNCTIONS
	else if (fn == 0x41) {
		mbuf[0] = settings.mbus_id;
		uint8_t X = mbuf[2];
		uint8_t N = mbuf[3];
		// BURN TEMP SETTINGS TO EEPROM
		if ((X == 0) && (N == 0)) {
			uint16_t eeprom_size = EEPROM_END - EEPROM_START;
			for (uint8_t i = 0; i < eeprom_size; i++) {
				EEPROM.write(i, *(uint8_t *)((uint32_t)(&data) + EEPROM_START + i));
			}
			serialBytes = 8;
			mbuf[3] = 2;
			memcpy(&mbuf[4], &eeprom_size, 2);
			swapByteOrder(mbuf, 4, 2);
		}
		// --END-- BURN TEMP SETTINGS TO EEPROM --END--

		// DO SOFTWARE RESET
		else if ((X == 1) && (N == 0)) {
			serialBytes = 6;
			mbuf[3] = 0;
			signDU(mbuf, serialBytes);
			mBusWrite(mbuf, serialBytes);
			softwareReset();
			return;
		}
		// --END-- DO SOFTWARE RESET --END--

		// DO LED ON
		else if ((X == 1) && (N == 1)) {
			digitalWrite(13, HIGH);
			serialBytes = 6;
			mbuf[0] = settings.mbus_id;
			mbuf[1] = fn;
			mbuf[2] = 0;
			mbuf[3] = 10;
		}
		// --END-- DO LED ON --END--

		// DO LED OFF
		else if ((X == 2) && (N == 1)) {
			digitalWrite(13, LOW);
			serialBytes = 6;
			mbuf[1] = fn;
			mbuf[2] = 0;
			mbuf[3] = 10;
		}
		// --END-- DO LED OFF --END--


		// IF FUNCTION DOES NOT EXIST
		else {
			serialBytes = 5;
			mbuf[1] = fn | 0x80;
			mbuf[2] = 0x01; // ILLEGAL FUNCTION
		}
		// --END-- IF FUNCTION DOES NOT EXIST --END--
		signDU(mbuf, serialBytes);
		mBusWrite(mbuf, serialBytes);
	}
	// --END-- USER FUNCTIONS --END--
	return;
}

void mBusWrite (uint8_t *buffer, int n) {
	digitalWrite(PIN_485, HIGH);
	Serial.write(buffer, n);
	//delay(ceil(settings.mbus_delay_a * n / settings.mbus_baud + settings.mbus_delay_b / settings.mbus_baud));
	digitalWrite(PIN_485, LOW);
}


void loop() {
	wdt_reset();
	if (Serial.available()) {
		mBus();
	}

	if (doloop && ready) {
		data.broke++;
		clear(1);
		return;
	}
	if (doloop) { // если пришло прерывание, то запускаем измерения
		calc();
		doloop = false; // когда записали в массив ждём новое прерывание
	}
	if ((millis() - time) >= settings.wait_trh && intCount > 1) { // если не было прерываний в течении TRH
		ready = true; // флаг готовности к передаче
	}
	if (intCount > (UINT16_MAX - 1)) {
		intCount = 1;
	}
}
