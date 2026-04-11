# 86switch_onoff_ai

Актуальна прошивка для ESP32-S3 4848S040 (480x480, LVGL), з погодою, радіо, SD-плеєром, мікрофонним донором, будильником і голосовими командами.

## Поточний стан інтерфейсу

1. `Screen1` (Home): погода, час/дата, швидкий запуск `Radio`, `Player`, `On`, `Off`.
2. `Screen10` (Settings): налаштування Wi-Fi/міста/режимів, кнопка переходу в медіаплеєр.
3. `Screen11` (Radio): керування інтернет-радіо станціями.
4. `Screen12` (Assistant/Sound): еквалайзер/VU, статус мікрофона, tap-to-listen.
5. `Screen13` (Alarm): будильник + вибір мелодій `1/2/3`.
6. Окремий екран `Media` (створюється кодом): SD-плеєр, прогрес, обкладинка альбому, перемикання джерела (SD/Radio).

### Вибір альбому в плеєрі

- У `Media` екрані додано селектор `Album` з кнопками `<` та `>`.
- Альбоми формуються автоматично з папок у `songs` (назва папки = назва альбому).
- `Next/Prev/Random` працюють у межах обраного альбому.
- Позиція треку (`xx/yy`) також рахується в межах обраного альбому.

## Актуальна логіка роботи

- Якщо активне `Radio` або `Player`, реакція на мікрофон блокується (щоб екран не будився від власного аудіо).
- Якщо медіа вимкнені і мікрофон-донор онлайн, система переходить у черговий режим прослуховування шуму.
- Автозатемнення/сон екрана: `3 хв` (`kDisplaySleepIdleMs = 180000` у `doMain.cpp`).
- Яскравість у сні та soft-off розведена окремими константами:
  - `kDisplaySleepBrightness`
  - `kSystemSoftOffBrightness`

## Основні модулі

- `86switch_onoff_ai.ino`: ініціалізація дисплея/тач/рендер-циклу.
- `doMain.cpp` / `doMain.h`: основна бізнес-логіка UI, станів, навігації, режимів.
- `radio_vendor.*`: інтернет-радіо.
- `sd_vendor.*`: SD-аудіо плеєр.
- `mic_remote_client.*`: робота з мікрофонною донор-платою.
- `weather_visuals.*` + `weather_sd_assets.*`: відображення іконок/анімацій погоди.
- `speaker_tone.*`: тон-генератор/будильник.
- `voice_command_parser.*` + `voice_command_ids.h`: голосові команди.

## Залежності

- ESP32 core for Arduino (`esp32:esp32`)
- LVGL
- Arduino_GFX
- ESP32 Audio (`Audio.h`)
- ArduinoJson
- GT911 touch library with header `Touch_GT911.h`

## Збірка (CLI)

```powershell
& "C:\Users\Taras\Documents\mou\tools\arduino-cli-bin\arduino-cli.exe" compile `
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,UploadMode=default,UploadSpeed=921600" `
  --build-path "C:\Users\Taras\Documents\mou\build_86switch_current" `
  "C:\Users\Taras\Documents\mou\86switch_onoff_ai"
```

Якщо бачиш помилку `Touch_GT911.h: No such file or directory`, встанови сумісну бібліотеку GT911, яка надає саме цей заголовок.

## Прошивка (CLI)

```powershell
& "C:\Users\Taras\Documents\mou\tools\arduino-cli-bin\arduino-cli.exe" upload `
  -p COM5 `
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,UploadMode=default,UploadSpeed=921600" `
  "C:\Users\Taras\Documents\mou\86switch_onoff_ai"
```

## Arduino IDE

1. Відкрити `C:\Users\Taras\Documents\mou\86switch_onoff_ai\86switch_onoff_ai.ino`.
2. Вибрати плату ESP32-S3 (16MB Flash, OPI PSRAM, `app3M_fat9M_16MB`).
3. Перевірити порт (`COMx`) і прошити.

## Що прибрано під час чистки

- Локальні build-теки всередині скетчу (`build*`).
- Старі backup-версії `doMain.cpp.*.bak`.
- Legacy-файли `main.cpp1`, `mic_vendor.*`, `WeatherNow.*`.
- Застарілі документи планування (`FINAL_ARCHITECTURE.md`, `PORTING_PLAN.md`).
