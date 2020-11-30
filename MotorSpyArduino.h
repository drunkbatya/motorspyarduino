#define		ARRSIZE		30
#define 	MBUFSIZE	128
#define		EEPROM_START	0
#define		EEPROM_END	27
#define		PIN_INT		2
#define		PIN_INT2	3
#define		PIN_485		4

#include	<EEPROM.h>
#include	<avr/wdt.h>

struct Settings { // holding registers	//|Size[B]|Memmory [B]|Reg|Comment
	uint16_t	state;		//| 002   |000..001   |000|контроль передачи
	uint16_t	id;		//| 002   |002..003   |001|id устройства
	uint16_t	mbus_id;	//| 002   |004..005   |002|id modbus
	uint32_t	mbus_baud;	//| 004   |006..009   |003|скорость обмена
	uint16_t	mbus_timeout;	//| 002   |010..011   |005|задержка
	float		mbus_delay_a;	//| 004   |012..015   |006|задержка
	float		mbus_delay_b;	//| 004   |016..019   |008|задержка
	uint16_t	hw_ver;		//| 002   |020..021   |010|версия железа
	uint16_t	fw_ver;		//| 002   |022..023   |011|версия прошивки
	uint32_t	flags;		//| 004   |024..027   |012|флаги
	uint16_t	wait_trh;	//| 002   |028..029   |014|время ожидания бездействия
};

struct Data { // input registers	//|Size[B]|Memmory [B]|Reg|Comment
	uint16_t	Array[ARRSIZE];	//| 060   |000..059   |000|массив
	uint16_t	divider;	//| 002   |060..061   |033|делитель
	uint16_t	broke;		//| 002   |062..063   |034|количество потеряных ссесий
};
