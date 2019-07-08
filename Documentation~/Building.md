# How to build and test


## Prerequisite

- Unity 2019.1 or greater
- git 2.11 or greater
- Visual Studio 2017 or 2019
- Make sure you've installed `Windows SDK 10.0.18362.0` via Visual Studio Installer


## Build and test

1. Clone and build native plugin
2. Open and test sample scene


### 1. Clone and build native plugin

Open command prompt and input the following commands:

```
git clone --config core.symlinks=true https://github.com/unity3d-jp/RaytracedHardShadow.git
cd RaytracedHardShadow
cd .RaytracedHardShadow\Plugin
.\build.bat
```


### 2. Open and test sample scene

- Open Unity project which is placed at `.RaytracedHardShadow`
- Open `Assets/Scenes/SampleScene`
- Enable `ShadowRaytracer` object in the hierarchy view
- Hit play button (or select "Edit > Play" )
- Select "Assets > Scenes > TestShadowBuffer" in the project view
- Check conent of Render Texture in the inspector
