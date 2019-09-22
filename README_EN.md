![demo](https://user-images.githubusercontent.com/1488611/60965577-1ca89700-a351-11e9-9b7d-962a6a7e1aed.png)
# Raytraced Hard Shadow
[English](https://translate.google.com/translate?sl=ja&tl=en&u=https://github.com/unity3d-jp/RaytracedHardShadow)


This plugin is for creating pixel-level precise hard shadows using raytracing. Combine this plugin with tools like [UnityChanToonShader](https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project) to create an anime-style shadow system. 

This plugin requires an environment that can also run DirectX Raytracing (DXR).
Specifically, **the Windows 10 1809 (October 2018 Update) or later and a GeForce 1070 or higher GPU are required**.  
As of 2019/07, only NVIDIA GPUs (GeForce or Quadro) support DXR. Also, some GeForce 1060 models support DXR and some do not, so be aware when attempting run this plugin on those GPUs (according to the list of [DXR Supported GPUs 2019/03](https://www.nvidia.com/content/dam/en-zz/Solutions/geforce/news/geforce-rtx-gtx-dxr/geforce-rtx-gtx-dxr-supported-gpus-march-2019.png), the GTX 1060 **6GB** supports DXR).
 
This plugin is compatible with Unity 2017.4 and later. The D3D11 (default) or D3D12 Unity graphics API is also required. 

## How-to Guide
- Download the package from [releases](https://github.com/unity3d-jp/RaytracedHardShadow/releases) import it into a Unity project. 
  - This github repository can also be directly imported into Unity 2018.3. To do so, open the project's Packages/manifest.json file in a text editor and add the following line to "dependencies".
  > "com.unity.raytracedhardshadow": "https://github.com/unity3d-jp/RaytracedHardShadow.git",

- Select Camera, then "Add Component" -> "UTJ/Raytraced Hard Shadow/Shadow Raytacer". This component will handle generating the shadows. 

<img align="right" src="https://user-images.githubusercontent.com/1488611/61529039-4b162880-aa5b-11e9-9a64-57429f21b8ce.png" width=400>

### Shadow Raytracer

#### Output
##### Generate Render Texture
When this is active, the texture's shadow texture resolution will automatically conform to the same resolution as the monitor.
If you would like an existing RenderTexture to be the output texture, disable this option and set the "Output Texture" manually.

**If the RenderTexture is a 32bit Int or a Uint, it will output as a bitmask**. If "Generate Render Texture" is active, set the "Output Type" to "Bit Mask" in order to output a bitmask.   
When outputting a bitmask, the "n-th" bit will correspond to the "n-th" light. For example, if a pixel recieves light from the 0-th and 2nd lights, (1 << 0) | (1 << 2) will lead to an output of 5. If a shader will handle shadows from multiple lights, it will differentiate the shadow from each light using this bitmask value.   
See ["Set Light Index To Alpha"](#set-light-index-to-alpha) for more details.

##### Assign Global Texture
When this is active, the shadow texture will act as a global shader parameter. The parameter's name will be the name set under the "Global Texture Name" setting. 

#### Shadows
##### Use Camera Culling Mask
When active, the Culling Mask assigned to the Camera will be applied when shadows are generated.
When inactive, the Camera's Culling Mask is ignored, but if "Use Light Culling Mask" is active the Light Culling Mask's effect will be applied.

##### Cull Back Faces
When active, culling will happen on the back face.

##### Ignore Self Shadow
This will ignore the effect of self shadows.   
If "Keep Self Drop Shadow" is enabled, and the distance at which a ray is obstructed is close to zero (can be adjusted with "Self Shadow Threshold"), the obstructing object will be ignored, but beyond that threshold objects will be considered shadows, even if it's the object itself.   

"Ignore Self Shadow" and "Keep Self Drop Shadow" are enabled by default, and is the recommended configuration.  
In order for HardShadow to clearly turn polygons into shadows, the edges of the self shadow need to be angular.   ハードシャドウはポリゴンの形状がはっきり影になるため、セルフシャドウの境界が角張ってしまいます。
そのため、セルフシャドウに関してはシェーディング (光源方向と法線から陰を判別) に任せた方がきれいな結果が得られます。
しかしそれだけだと、例えば鼻や耳が顔に落とす影が出なくなってしまいます。同じセルフシャドウでも落ち影は保った方がいいでしょう。
デフォルト設定はこれらの要件を満たすものになっています。
![Self Shadow Options](https://user-images.githubusercontent.com/1488611/61041749-d402e380-a40d-11e9-8698-ef0eedea7770.png)

#### Lights
##### Use Light Shadow Settings
有効な場合、Light の "Shadow Type" が "No Shadows" のものは無視します。  
"Soft Shadows" と "Hard Shadows" は区別しません。どちらの場合も同じ処理で影テクスチャを生成します。

##### Use Light Culling bitmask
有効な場合、Light に設定されている Culling Mask をそのまま影生成時にも適用します。  
無効な場合その Light の Culling Mask は無視しますが、"Use Camera Culling Mask" が有効な場合 Camera 側の Culling Mask の影響は受けます。

##### Set Light Index To Alpha
有効な場合、Light のインデックスを alpha に設定します。  
影バッファを bitmask として出力した場合、この値を用いてライトと影の bit の関連付けを行います。
最初のライトが 1000、以降 2000, 3000... と続きます。(デフォルトの alpha が 1 であるため、無関係なライトと混ざらないようにするために 1000 を初期値としています)  
Legacy Forward の場合、シェーダ側では_LightColor0.aでこの値を取れます。  
["Generate Render Texture"](#generate-render-texture) も参照ください。また、**デフォルトでは無効** である点にご注意ください。

##### Light Scope
"Entire Scene" の場合、シーン上の全ライトを用います。"Scene" の場合指定のシーン内に存在するライトを用います。"Objects" の場合指定オブジェクトのみ用います。  
エリアライトは非サポートであり、無視されます。

#### Geometry
##### Use Object Shadow Settings
有効な場合、MeshRenderer / SkinnedMeshRenderer の "Cast Shadows" と "Receive Shadows" を影設定に適用します。  
"Cast Shadows" が Off であれば影をキャストせず、Shadows Only であればカメラには映らず影だけキャストする、といった具合です。
無効な場合、常に影のキャストとレシーブ両方を有効にします。

##### Geometry Scope
影のキャスト、レシーブを行うオブジェクトを指定します。こちらも Lights 同様、全シーン、シーン単位、オブジェクト単位での指定が可能です。オブジェクト指定の場合、指定オブジェクトの子オブジェクトも含められます。

#### Misc
##### GPU Skinning
スキニングおよびブレンドシェイプを GPU で行い、高速化を図るオプションです。  
本プラグインは Unity 側とは独立して Mesh データを持っており、このオプションが有効だとスキニングも独立して行うようになります。
もし Unity 側と影側でモデルの不一致が確認された場合、このオプションを無効化すると改善される可能性があります。(不一致 = 本プラグインの不具合 でもあるため、再現できるデータと一緒にレポートいただけるととても助かります)  
無効な場合、Unity 側で bake した結果をプラグインに送ります。そのため Mesh データは確実に正確になりますが、代償として大幅に遅くなります。

##### Adaptive Sampling
これが有効な場合、若干のクオリティの低下と引き換えにレンダリングの大幅な高速化を行います。  
どれくらい速くなるかシーンの複雑さに左右されますが、冒頭の画像のシーン ([Unity-Chan CRS](https://github.com/unity3d-jp/unitychan-crs)) で概ね 3 倍以上速くなっています (18ms -> 5ms)。
クオリティの低下とは具体的には、細かいディティールが潰れてしまいます。例えば 2 ピクセル刻みの網目模様のような箇所が真っ黒になるといった現象が発生します。  
作業中は有効にしてプレビューを高速化し、最終出力の際は切る、といった使い方が考えられます。

##### Antialiasing
アンチエイリアシングをかけます。  
トゥーンシェーダと併用する場合、これは望ましくない結果を招くと思われます。(オブジェクトの境界付近に汚れが出るなど) 
影バッファをそれ単独で絵として使いたいような特殊なケース用のオプションです。

## 制限事項、既知の問題など
- レンダリング対象は現在 MeshRenderer と SkinnedMeshRenderer のみ対応しています。
  - ParticleSystem、Terrain などは未対応です。
- アルファテスト、半透明、Stencil による切り抜きは未対応です。
  - これらは将来的に対応する可能性があります。
- シェーダ内でモデルが変化する処理を入れている場合、それは影側には反映されず不一致が発生します。
  - 頂点シェーダで特殊な変形を行っている場合や、Geometry Shader や Tessellation でポリゴンの増減を行っている場合などが該当します。
  - これらは対応が難しく、将来的にも対応する見込みは薄いです。

## ライセンス
[Unity Companion License](LICENSE.md) 
