# Android APK 构建与验证

当前分支面向 Android 8.0（API 26）及以上的 ARM64 手机。游戏内容随 APK 分发，安装后可离线游玩。应用包名为 `com.akil0814.flyme2themoon`，版本为 `1.0.1`（versionCode 2）。本次交付 APK，不包含商店发布流程。

## Windows 构建

使用 PowerShell 7 和 JDK 21。将 `JAVA_HOME` 指向 JDK 根目录，在项目根目录执行：

```powershell
./android/tools/setup-android.ps1 -AcceptLicenses
./android/tools/new-release-key.ps1
./android/tools/test-host.ps1
./android/tools/build-android.ps1 -Variant All
```

首次准备会下载 Google Android 命令行工具，并安装 SDK Platform 36、Build Tools 35.0.0、NDK 28.2.13676358 和 CMake 3.31.6。Gradle Wrapper 固定为 8.13，AGP 固定为 8.12.2。SDK 默认位于忽略的 `out/android/sdk`；可以向准备脚本传入 `-SdkRoot`。构建依赖首次下载后，资源准备和游戏运行不需要联网。

仅调试时使用 `-Variant Debug`，无需发布密钥。正式构建使用 `-Variant Release` 或 `All`，签名缺失会失败，不会自动换用 Debug 签名。Debug 与 Release 使用同一包名、不同签名；在手机上互相切换需要先卸载，卸载会删除应用数据。日常验证优先安装正式签名版本。

输出位于 `out/android/dist`：Debug/Release APK、各 APK 的 SHA-256、签名证书信息和元数据，以及 Release 原生符号 ZIP。`verify-apk.ps1 -Apk <路径>` 可单独验证签名、ARM64 架构、API、包内资源校验和、ELF 段与 APK 的 16 KiB 对齐。

## 发布密钥

默认签名目录为 `%LOCALAPPDATA%/FlyMe2TheMoon/signing`，包含项目专用 keystore 和本地 `signing.properties`。脚本限制目录为当前 Windows 用户访问，不打印密码，也不将秘密写入 Git。必须安全备份整个目录；后续覆盖安装必须使用相同密钥。

密钥已存在时不要再次运行生成脚本。其他机器可使用 `build-android.ps1 -SigningProperties <本地文件>`；直接执行 Gradle 时通过 `-PflySigningProperties=<本地文件>` 或 `FLY_SIGNING_PROPERTIES` 指定。配置字段为 `storeFile`、`storePassword`、`keyAlias`、`keyPassword`，本地绝对路径使用正斜杠。密码不得放入命令行参数、提交内容或共享日志。

每次正式更新递增 versionCode；versionName 为面向玩家的版本号。版本定义在 Android app 的 Gradle 配置中。构建产物名称与版本定义同步调整。

## 安装与操作

```powershell
./out/android/sdk/platform-tools/adb.exe devices
./out/android/sdk/platform-tools/adb.exe install -r ./out/android/dist/FlyMe2TheMoon-1.0.1-arm64-release.apk
./out/android/sdk/platform-tools/adb.exe shell am start -n com.akil0814.flyme2themoon/.BootstrapActivity
```

手机需要允许这台电脑进行 USB 调试。也可以直接将 APK 复制到手机上安装。

- 游戏固定横屏，以 1280×720 逻辑画面等比显示，留边不参与操作。
- 按住屏幕位置操控飞行，松手停止推力；按钮、滑块和菜单直接触摸。
- 同时只接受一个触点，额外手指不抢占；滑出游戏区域、取消和切换场景会清除按住状态。
- 系统返回在关卡中打开暂停，再次返回继续；菜单返回上级，主菜单返回弹出退出确认。
- 切后台暂停音频与游戏推进；回到关卡后保持暂停，点击继续。系统结束进程后恢复已保存进度，不恢复关卡中途状态。
- 设置保留语言、音量、帧率和 VSync，Android 版不提供桌面窗口设置与 F2 开发面板。

## 资源与数据

Gradle 将正式 `assets/` 生成 ZIP 与 SHA-256 清单。首次启动由原生准备页面在后台线程复制、解压并逐文件校验，完成后才进入 SDL。应用内部 `files/content/<内容哈希>/assets` 保存资源；`files/runtime/player_data` 保存设置和存档，`files/runtime/logs` 保存日志。

再次启动检查版本、完成标记以及文件存在性和大小，不重新计算整套资源哈希。缺失或长度异常会重新准备；完整哈希在解包时校验。更新先准备新版本，成功后清理旧版本，玩家数据不在清理范围内。准备失败可重试，不会启动不完整内容。

首次解包额外占用约 145 MiB，更新时需要保留旧资源并准备新资源，须留出相应空间。资源不放在可被系统清理的 cache 目录，也不申请外部存储权限。清除应用数据或卸载会删除存档。

## 验证边界

`test-host.ps1` 在主机 JVM 上验证资源安装/复用/更新/中断/校验/空间不足，安装 MSVC 时还验证触点状态机。APK 构建验证不等于真机运行验证。

每次发布应检查首次启动、菜单/选关、剧情教学、飞行、暂停/重试/结算、音量语言保存、后台恢复、锁屏恢复，以及同密钥覆盖更新后存档保留。另检查全部 60 关资源依赖。真机结果和未验证项记录在 `out/android/validation.md`，故障日志可通过 `adb logcat -s FlyMe2TheMoon SDL SDL/APP AndroidRuntime` 获取。
