# Android
## Getting Started
The following guide assumes that you are on a Linux or Unix like system.
### Environment
To build Calvin Constrained for Android one has to follow the following steps.

1. Install Android tools.
* The project depends on Android studio and CMake, begin by installing Android studio from [here](https://developer.android.com/studio/index.html).
* For easy use of the Android command line tools, add the \<android-sdk\>/platform-tools directory to your path by adding the following to your .bashrc file.
```
PATH=$PATH:<android-sdk-directory>/platform-tools
PATH=$PATH:<android-sdk-directory>/tools
```
replace \<android-sdk-directory\> by the installation directory of the Android SDK (usually ~/Android/Sdk).
2. Install the Android NDK build tools.
* run ```android``` to open the SDK-manager.
* select the following packages for installation (or update).
    * Android SDK Tools
    * Android SDK Platform-tools
    * Android SDK Build-tools
    * Android 6.0 -> SDK Platform
    * NDK Bundle
    * Google Repository
* Under SDK Tools, install the following packages
    * CMake
    * LLDB
    * NDK
* Click install packages and accept the license agreement to install the tools.
* Open Android Studio and import the Android project in \<calvin-constrained-directory\>/platforms/android/calvin_constrained.

### Configuration
#### Socket
If you are using sockets as a transport mechanism, open the build configuration file app.gradle and add the cFlags tag if it does not exist.
```
cFlags "-DTRANSPORT_SOCKET"
```

#### FCM
1. If you are using FCM as a transport mechanism, open the file app.gradle and add the cFlags tag.
```
cFlags "cFlags "-DTRANSPORT_FCM"
```
2. Create a Google Firebase API key.
* Go to the [Google Firebase console](https://console.firebase.google.com/).
* Create a project and name it something unique.
* Click Add Firebase to your Android app.
* Enter ```ericsson.com.calvin.calvin_constrained``` as the package name.
* In the next step, download the google-services.json configuration file copy it to the Android project as described. If the file already exists, replace it with the one you have downloaded.

#### Runtime configuration
1. Open the file CalvinService.java.
2. In the class CalvinService and method onStartCommand() the configuration parameters are set.
3. Enter the name of the runtime and the list of proxy_uris separated by a ",". For FCM add an URI on the form.
```
calvinfcm://[sender id]:*
```
where [sender id] is your firebase project number. The project number can be found in the Firebase Console or in the downloaded FCM configuration file google-services.json under "project number".

### Build
1. Build Micropython for Android by running the following from the root of the Calvin Constrained repo.
```
platform/android/build_android.sh \<android-sdk-directory\>/ndk-bundle/ [optional platform parameter]
```
And replace \<android-sdk-directory\> with your installation path of the SDK. If you are only building for a certain platform, enter the platform name as a second parameter. To build for all platforms, ignore the parameter.
Supported platforms are.
* arm
* arm64
* x86
* x86_64
* mips
* mips64
2. Build Calvin Constrained and the Android specific Java files.
* Before the first time you are building, reload the project files by choosing Build->Refresh Linked C++ Projects.
* To build the project choose Build -> Make Project.

### Run on Physical device
1. Enable FCM and set the FCM secret by adding them to Calvin Base configuration:
```
{
  "global": {
    "transports": ["calvinip", "calvinfcm"],
    "fcm_server_secret": "[you secret]"
                                                }
}
```
2. Start a Calvin Base runtime listening on FCM connections:
```
csruntime --host [your ip] --port 5000 --controlport 5001 --uri calvinfcm://[sender id]:*
```
and replace [sender id] with your Firebase project number.
3. Make sure your phone is in develop mode. If it is not, open settings->about and tap multiple times on the build-number until you are a developer.
4. Make sure your phone has the developer options turned on in settings->developer options.
3. Connect your device to the computer.
4. Run ```adb devices``` and make sure your device is visible and active (you may get a prompt on your phone to accept the computer).
5. Build, install and run the app from Android Studio by choosing Run -> Run 'App'.
