![demo](https://user-images.githubusercontent.com/1488611/60965577-1ca89700-a351-11e9-9b7d-962a6a7e1aed.png)
# Raytraced Hard Shadow
レイトレーシングにより、ピクセル単位の正確なハードシャドウを生成するプラグインです。[UnityChanToonShader](https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project) などと併用することを想定した、アニメ用影システムとなります。

実行には DirectX Raytracing (DXR) が動作する環境が必要です。
具体的には、**Windows 10 の 1803 (April 2018 Update) 以降、GeForce 1070 以上の GPU が必須** になります。  
 2019/07 現在、NVIDIA の GPU (GeForce および Quadro) しか DXR をサポートしていません。また、GeForce 1060 には DXR をサポートしているモデルとしていないモデルが混在しており、同 GPU でも動く可能性はあります。
 
Unity 2017.4 以降 で動作します。Unity 側のグラフィック API は D3D11 (デフォルト)、もしくは D3D12 である必要があります。

## 使い方
- [releases](https://github.com/unity3d-jp/RaytracedHardShadow/releases) からパッケージをダウンロードし、Unity のプロジェクトにインポート。
  - Unity 2018.3 以降の場合、この github リポジトリを直接インポートすることもできます。プロジェクト内にある Packages/manifest.json をテキストエディタで開き、"dependencies" に以下の行を加えます。
  > "com.utj.raytracedhardshadow": "https://github.com/unity3d-jp/RaytracedHardShadow.git",

- Camera を選択し、"Add Component" -> "UTJ/Raytraced Hard Shadow/Shadow Raytacer" を選択。このコンポーネントが影生成を担当します。

<img align="right" src="https://user-images.githubusercontent.com/1488611/60966118-7b224500-a352-11e9-8160-4c846ff38443.png" width=400>

### Shadow Raytracer

###### Generate Render Texture
これが有効な場合、出力先の影テクスチャを画面の解像度に合わせて自動的に作成/更新します。
既存の RenderTexture を出力先としたい場合、このオプションを無効化して "Output Texture" を手動で設定します。

##### Assign Global Texture
これが有効な場合、影テクスチャをグローバルなシェーダパラメータとして設定します。パラメータ名は "Global Texture Name" に設定した名前になります。

##### Cull Back Faces
有効な場合、裏面カリングを行います。

##### Ignore Self Shadow
自分自身に落とす影 (セルフシャドウ) を無視します。  
"Keep Self Drop Shadow" が有効な場合、レイが遮られるまでの距離がほぼゼロ ("Self Shadow Threshold" で調節可) であればその遮ったオブジェクトは無視しますが、それ以上であれば自分自身であっても影とみなします。  

デフォルトでは "Ignore Self Shadow" "Keep Self Drop Shadow" 共に有効となっており、通常これが最も望ましい動作であると思われます。  
ハードシャドウはポリゴンの形状がはっきり影になるため、セルフシャドウの境界が角張ってしまいます。
そのため、セルフシャドウに関してはシェーディング (光源方向と法線から陰を判別) に任せた方がきれいな結果が得られます。
しかしそれだけだと、例えば鼻や耳が顔に落とす影が出なくなってしまいます。同じセルフシャドウでも落ち影は保った方がいいでしょう。
デフォルト設定はこれらの要件を満たすものになっています。
![Self Shadow Options](https://user-images.githubusercontent.com/1488611/61041749-d402e380-a40d-11e9-8698-ef0eedea7770.png)

##### Lights
"Light Scope" が "Entire Scene" の場合、シーン上の全ライトを用います。"Scene" の場合指定のシーン内に存在するライトを用います。"Objects" の場合指定オブジェクトのみ用います。  
エリアライトは非サポートであり、無視されます。

##### Geometry
影のキャスト、レシーブを行うオブジェクトを指定します。こちらも Lights 同様、全シーン、シーン単位、オブジェクト単位での指定が可能です。オブジェクト指定の場合、指定オブジェクトの子オブジェクトも含められます。  
"Separate Casters And Receivers" を有効にすると、影を受けるオブジェクトとキャストするオブジェクトを別に設定できます。この場合、影のキャスト/レシーブを行う組み合わせ (レイヤー) を設定するフィールドが現れます。レイヤーは最大 7 枚まで設定可能です。  


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
- アルファテストや半透明は未対応です。

## 開発者向け
プラグインのビルド手順: [Building.md](Documentation~/Building.md)

## ライセンス
[MIT](LICENSE.txt)
