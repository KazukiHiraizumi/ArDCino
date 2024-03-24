# MbedOS(libmbed.a)のビルド方法

## 機器構成
1. ボード Arduino Nano33 BLE(Nordic製SoC nRF52840)
2. デバッガ CMSIS-DAP(手製)

## libmbed.aとは
libmbed.aはmbedOSのコアモジュールです。mbedOSだけでなく小さなシステム用の最近の組み込みOSは、OSとアプリが分離しておらず、ライブラリとしてアプリにリンクして書き込みます(大昔の大型コンピュータのような形態です)。

## ArduinoIDE  
ArduinoIDE(1.8)がなければインストします。ArduinoIDEのメニューから、ツール&rarr;ボード&rarr;ボードマネージャを開き、**Arduino Mbed OS Nano Boards** を追加します。これにて**~/.arduino15/packages/arduino/hardware/mbed_nano/**以下にコンパイルに必要なコードが取り込まれます。

## mbedOSソースをclone
ARMが搭載されたArduinoには、実はウラでmbedOSが動いています(Nano33,RP2040,Seeed...)。このソースをCloneします
~~~
git clone https://github.com/arduino/ArduinoCore-mbed.git  
~~~

ここに<a href="Mbed_Hack.md">Mbed_Hack</a>のパッチを当てます。

## ビルド
ラッパーのソースの以下ディレクトリに、re;buildコマンドがあるので、ここにcdします。
~~~
cd ~/.arduino15/packages/arduino/hardware/mbed_nano/4.0.10/
~~~
mbedOSのソースは、デフォルトではARM-mbedからcloneしてきます。  
先にパッチしたソースを使うには、"-r"オプションにてそのローカルディレクトリを指示し、以下のコマンドでビルドします、
~~~
PROFILE=release ./mbed-os-to-arduino -r ~/mbed-os\(arm-mbed\) SEEED_XIAO_NRF52840:SEEED_XIAO_NRF52840
~~~
