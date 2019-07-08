# Raytraced Hard Shadow
レイトレーシングにより正確なハードシャドウを生成するプラグインです。主に [UnityChanToonShader](https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project) と併用することを想定した、アニメ用影システムとなります。

実行には DirectX Raytracing (DXR) が動作する環境が必要です。
具体的には、**Windows 10 の 1803 (April 2018 Update) 以降、GeForce 1070 以上の GPU が必須** になります。  
 2019/07 現在、NVIDIA の GPU (GeForce および Quadro) しか DXR をサポートしていません。また、GeForce 1060 には DXR をサポートしているモデルとしていないモデルが混在しており、同 GPU でも動く可能性はあります。
 
Unity 2017.4 以降 で動作します。Unity 側のグラフィック API は D3D11 (デフォルト)、もしくは D3D12 である必要があります。

## 使い方
- [releases](https://github.com/unity3d-jp/RaytracedHardShadow/releases) からパッケージをダウンロードし、Unity のプロジェクトにインポート。
  - Unity 2018.3 以降の場合、この github リポジトリを直接インポートすることもできます。プロジェクト内にある Packages/manifest.json をテキストエディタで開き、"dependencies" に以下の行を加えます。
  > "com.utj.raytracedhardshadow": "https://github.com/unity3d-jp/RaytracedHardShadow.git",

- Camera を選択し、"Add Component" -> "UTJ/Raytraced Hard Shadow/Shadow Raytacer" を選択。このコンポーネントが影生成を担当します。

### Shadow Raytracer

###### Generate Render Texture
これが有効な場合、出力先の影テクスチャを画面の解像度に合わせて自動的に作成/更新します。
既存の RenderTexture を出力先としたい場合、このオプションを無効化して "Output Texture" を手動で設定します。

##### Assign Global RenderTexture
これが有効な場合、影テクスチャをグローバルなシェーダパラメータとして設定します。パラメータ名は "Global Texture Name" に設定した名前になります。

##### Cull Back Faces
有効な場合、裏面カリングを行います。

##### GPU Skinning
スキニングおよびブレンドシェイプを GPU で行い、高速化を図るオプションです。  
本プラグインは Unity 側とは独立して Mesh データを持っており、このオプションが有効だとスキニングも独立して行うようになります。
もし Unity 側と影側でモデルの不一致が確認された場合、このオプションを無効化すると改善される可能性があります。(不一致 = 本プラグインの不具合でもあるため、再現できるモデルと一緒にレポートいただけるととても助かります)  
無効な場合、Unity 側で bake した結果をプラグインに送ります。そのため Mesh データは確実に正確になりますが、代償として大幅に遅くなります。

##### Adaptive Sampling
これが有効な場合、若干のクオリティの低下と引き換えにレンダリングの大幅な高速化を行います。作業中はこれを有効にしてプレビューを高速化し、最終出力の際は切る、などの使い方が考えられます。  
クオリティの低下とは具体的には、細かいディティールが潰れてしまいます。例えば 2 ピクセル刻みの網目模様のような箇所が真っ黒になるなどです。

##### Antialiasing
アンチエイリアシングをかけます。  
トゥーンシェーダと併用する場合、これは望ましくない結果を招くと思われます。(オブジェクトの境界付近に滲みが出る) 
影バッファをそれ単独で絵として使いたいような特殊なケース用となります。

## 制限事項、既知の問題など
- レンダリング対象は現在 MeshRenderer と SkinnedMeshRenderer のみ対応しています。
  - ParticleSystem、Terrain などは未対応です。
- アルファテストや半透明は未対応です。

## ライセンス
[MIT](LICENSE.txt)
