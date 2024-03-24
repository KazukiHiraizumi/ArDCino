# Mbed OSをハックする
MbedOSをDCに導入するにあたり、DC特有の用途からくる問題点がある。それは起動(電源リセットからmainに制御が来るまで)のディレイである。デバッガでArduinoに直接HEXを書き込んでも、起動に0.2〜0.3秒の遅れがあるので、これはBootloaderのディレイではなく、Mbed-OS(libmbed)のディレイである。  
この問題を解決するため、Mbed-OSをハックし、コードを改変したので、その過程を以下に記す。

## 機器構成
1. ボード Arduino Nano33BLE(Nordic製SoC nRF52840搭載)
2. 開発環境 VS-Code
3. デバッガ CMSIS-DAP

## VScodeをカスタマイズ
VScodeの拡張機能でMbedOSの開発環境を作る。以下参照。
https://github.com/ARMmbed/mbed-os-5-docs/blob/5.4/docs/debugging/vscode.md

## Mbed OSのサンプル  
Exampleプロジェクトがあるので、これをclone。VSCode環境でビルド＆デバッグが出来るはず。
~~~
git clone https://github.com/ARMmbed/mbed-os-example-blinky.git
~~~
cloneするとOSのソースコードもプロジェクトフォルダ下に取り込まれる(mbed-osディレクトリ下)。

## ブートシーケンス
<img src="https://os.mbed.com/docs/mbed-os/v6.16/program-setup/images/boot_sequence.png" height="100%" />
OSSであるMbedOSでは、ブートシーケンスが公開されている。これを手掛かりに、どこでクロックを消費しているかを見つける。

> コード解析の基本は、**What**ではなく**Where**

## サイクルカウンター
~~~
DWT->CYCCNT=0;
DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
~~~
どこでクロックを消費しているかを見つけるため、デバッグレジスタを使って命令サイクルを見る。SystemInitに追加することで、起動時から積算命令サイクルを見れる。

## ブレークポイント(BP)
通常のデバッグではBPをいくつか設定するが、リアルタイム性の調査では１箇所のみに入れる。リセットから最初のBPまでは一気に動かす。さもないとCPUクロックに依存しないデバイスの影響で混乱する。

## 問題箇所の発見
1. カーネル起動／mbed_rtos_start::mbed_rtos_rtx.c
~~~
MBED_NORETURN void mbed_rtos_start()
{
    ...........

    osThreadId_t result = osThreadNew((osThreadFunc_t)mbed_start, NULL, &_main_thread_attr);
    if ((void *)result == NULL) {
        MBED_ERROR1(MBED_MAKE_ERROR(MBED_MODULE_PLATFORM, MBED_ERROR_CODE_INITIALIZATION_FAILED), "Pre main thread not created", &_main_thread_attr);
    }

    osKernelStart();
    MBED_ERROR(MBED_MAKE_ERROR(MBED_MODULE_PLATFORM, MBED_ERROR_CODE_INITIALIZATION_FAILED), "Failed to start RTOS");
}
~~~
この関数と、シーケンス上の次の関数mbed_startの間でクロックを大量に消費していることが分かった。ブートシーケンスのここまでは、リセットベクタ(Reset_Handler)のコンテキストで実行されるが、このコンテキスがmbed_rtos_startに入り、osKernelStartに移るとそからは戻って来ない。つまりリセットから続いたコンテキストはここでスレッドのスケジューラになる(つまりカーネル)。従ってシーケンス上の次の処理であるmbed_startはスレッドのひとつとして実行される。
通常Mbedのスレッド切り替えは、最悪でも100&micro;sec以内なので、カーネル自体は問題なさそう。

2. タイマー init_os_timer:mbed_os_timer.cpp
~~~
OsTimer *init_os_timer()
{
    // Do not use SingletonPtr since this relies on the RTOS.
    // Locking not required as it will be first called during
    // OS init, or else we're a non-RTOS single-threaded setup.
    if (!os_timer) {
#if DEVICE_LPTICKER && !MBED_CONF_TARGET_TICKLESS_FROM_US_TICKER
        os_timer = new (os_timer_data) OsTimer(get_lp_ticker_data());
#elif DEVICE_USTICKER
        os_timer = new (os_timer_data) OsTimer(get_us_ticker_data());
#else
        MBED_ERROR(
            MBED_MAKE_ERROR(
                MBED_MODULE_PLATFORM,
                MBED_ERROR_CODE_CONFIG_UNSUPPORTED),
            "OS timer not available");
#endif
    }

    return os_timer;
}
~~~
さらにデバッガでトレースすると、上記の
~~~
os_timer = new (os_timer_data) OsTimer(get_lp_ticker_data());
~~~
でクロックを消費していることが分かった。これはカーネルタイマーを初期化している箇所で、スケジューラにはタイマーが必須である。
カーネルタイマーはIFディレクティブでどちらかを選べるが、デフォルトは上の方(LPTICKER)でビルドされる。

3. LPTICKER
LPTICKERは外付けの低消費電力オシレータからのクロックと分かった。これはCPUをスリープにすると内部クロックも止まってしまうので、定周期でWakeUpする必要がある場合は困る。このためnRF52xxxでは、外付けのタイマーが組み込まれている。デフォルトでは外部タイマーを使うようにビルドされるが、OsTimerは、このクロックが安定するのを待つので、これが起動遅れの原因となっている。

## パッチ当て
問題箇所が特定できたので、次はその解決のためパッチをあてる。

1. target.json
デイレクトリはmbed-os/targets/。この中の"MCU_NRF52840"の"override"プロパティに追加
~~~
        "overrides": {
            "mpu-rom-end": "0x1fffffff",
            "tickless-from-us-ticker": true,
            "init-us-ticker-at-boot": true
        },
~~~
これにてカーネルタイマーにUSTIMERが選択される。
2. system_nrf52840.c
ディレクトリはmbed-os/targets/TARGET_NORDIC/TARGET_NRF5x/TARGET_NRF52/TARGET_MCU_NRF52840/device/(深い…)。こちらはベタ書きなので、すべてコメントアウトする。
~~~
// Wait for the external oscillator to start up.
//  while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {
//      // Do nothing.
//  }
~~~

## 確認
