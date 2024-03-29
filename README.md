# DIY Analog pad(Pico version)

![photo1](https://github.com/nicotakuya/diy_analogpad_pico/assets/5597377/d90262b4-ec53-4c2f-90be-67729fe362ce)

## Overview

「DIY Analog pad」のRaspberry Pi Pico版です。現時点ではMDモードのみ対応しています。

## Files

・diyapadpico.ino: ソースファイル。開発環境はArduino IDE(Arduino-Pico)。

・8x8font.h : フォントデータ。

・diy_analogpad_pico_schematics.png : 回路図。

・STL_file : 3Dプリンタ用データ

![stl_file](https://github.com/nicotakuya/diy_analogpad_pico/assets/5597377/138cb32d-592b-45b2-b06d-d1b1ccf1db98)

## Parts

・U1: Raspberry Pi Pico

・U2: 8ビット双方向ロジックレベル変換ブレークアウトモジュールキット(秋月電子通商)

・U3: OLED Display SSD1306

・VRX/VRY: 右スティック RKJXV122400R

・VLX/VLY: 左スティック RKJXV122400R

・SW1-SW8: Switch

・CN1: Dsub9pin female

・D1 : ショットキーバリアダイオード

・JP1:LEFT/RIGHTを左スティックに割り振る場合はショート

・JP2:LEFT/RIGHTを右スティックに割り振る場合はショート

・JP3:MD modeの場合はショート

・JP4:PC modeの場合はショート

## Movie

https://www.youtube.com/watch?v=bPYklQEjaYo
