![demo](https://user-images.githubusercontent.com/1488611/60965577-1ca89700-a351-11e9-9b7d-962a6a7e1aed.png)
# Raytraced Hard Shadow
[Japanese](Documentation~/jp/index.md)


This plugin is for creating pixel-level precise hard shadows using ray tracing. Combine this plugin with tools like [UnityChanToonShader](https://github.com/unity3d-jp/UnityChanToonShaderVer2_Project) to create an anime-style shadow system. 

This plugin requires an environment that can also run DirectX Raytracing (DXR).
Specifically, **the Windows 10 1809 (October 2018 Update) or later and a GeForce 1070 or higher GPU are required**.  
As of 2019/07, only NVIDIA GPUs (GeForce or Quadro) support DXR. Also, some GeForce 1060 models support DXR and some do not, so be aware when attempting to run this plugin on those GPUs (list of [DXR Supported GPUs 2019/03](https://www.nvidia.com/content/dam/en-zz/Solutions/geforce/news/geforce-rtx-gtx-dxr/geforce-rtx-gtx-dxr-supported-gpus-march-2019.png), the GTX 1060 **6GB** supports DXR).
 
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
When outputting a bitmask, the "n-th" bit will correspond to the "n-th" light. For example, if a pixel receives light from the 0-th and 2nd lights, (1 << 0) | (1 << 2) will lead to an output of 5. If a shader will handle shadows from multiple lights, it will differentiate the shadow from each light using this bitmask value.   
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

"Ignore Self Shadow" and "Keep Self Drop Shadow" are enabled by default, and this is the recommended configuration.  
In order for HardShadow to precisely turn polygons into shadows, the edges of the self shadow need to be angular. 
Therefore, allowing shading (determining shadows based on light source direction and vectors) to handle self shadows will yield the best results. 
However, this can also lead to cases like shadows that should fall around the nose and ears not displaying. Even for self shadowing you'll want to preserve normally cast shadows. 
This is how to do so with default settings. 
![Self Shadow Options](https://user-images.githubusercontent.com/1488611/61041749-d402e380-a40d-11e9-8698-ef0eedea7770.png)

#### Lights
##### Use Light Shadow Settings
When active, objects with a Light "Shadow Type" of "No Shadows" will be ignored.  
"Soft Shadows" and "Hard Shadows" will not be differentiated, and shadow textures for both will be generated with the same process.

##### Use Light Culling bitmask
When active, the Culling Mask assigned to Lights will also be applied when shadows are generated.  
When disabled the Culling Mask will be ignored, but if "Use Camera Culling Mask" is active then the Camera's Culling Mask effects will still be applied. 

##### Set Light Index To Alpha
When active, the Light's index will be assigned to the alpha.  
If the shadow buffer is output as a bitmask, lights with that value will be associated with the shadow's bits.
The first light will have a value of 1000, and subsequent lights will be 2000, 3000...etc. The numbers start at 1000 to avoid mixing unrelated lights, because the default alpha has a value of 1.  
When using Legacy Forward rendering, _LightColor0.a will take this value on the shader side.  
See ["Generate Render Texture"](#generate-render-texture) for more details. Keep in mind that **this setting is disabled by default**.

##### Light Scope
When set to "Entire Scene", all lights in the scene will be used. When set to "Scene" only the designated lights in the scene will be used. When set to "Objects" only the designated object(s) will be used.  
Area lights are not supported, and will be ignored.

#### Geometry
##### Use Object Shadow Settings
When active, "Cast Shadows" and "Receive Shadows" from the MeshRenderer / SkinnedMeshRenderer will be applied to the shadow settings.  
If "Cast Shadows" is off shadows won't be cast, if Shadows Only is off, shadows will be cast but won't show up in the camera. 
When disabled, shadows will be both cast and received.

##### Geometry Scope
This setting determines which objects will cast and receive shadows. As with Lights, the scope can be set to include "all in every scene", "all in specified scene" or "those specified in a scene". When set to "objects", this will also include child objects.

#### Misc
##### GPU Skinning
Doing skinning or blendshapes on the GPU is an option for increasing your content's speed.   
This plugin holds Mesh data independent of Unity, so enabling this option will also cause skinning to occur independently. 
If there is a mismatch in the model between Unity and the shadows, disabling this option may resolve the issue (the mismatch may be caused by an issue with the plugin, so report any bugs encountered along with data that can be used to recreate the issue).   
When this option is disabled, the shadows will be baked in Unity and the results will be sent to the plugin. This will keep the Mesh data correct, but at the cost of significant slowdown. 

##### Adaptive Sampling
This option will generally increase the rendering speed, at the cost of a drop in quality.  
The speed increase depends on the complexity of the scene, but will generally be three times faster at the initial images in the scene ([Unity-Chan CRS](https://github.com/unity3d-jp/unitychan-crs)) (18ms -> 5ms).
The drop in quality will generally mean the loss of small details, for example a mesh pattern with two pixel-wide gaps may appear completely black.  
This feature is most useful when working on content in order to speed up previewing, and should be disabled before the final output stage. 

##### Antialiasing
Enable antialiasing.   
This may cause undesirable results when paired with a toon shader (the area around object borders may look messy).
This option is ideal for special cases such as wanting to use the shadow buffer alone as a drawing. 

## Limitations and Known Issues
- Only MeshRenderer and SkinnedMeshRenderer are supported as rendering targets.
  - ParticleSystem, Terrain, etc. are not supported.
- Alpha test, semi-transparence, and clipping with Stencil are not supported.
  - Support may be provided in the future. 
- If processes that change the model are included in the shader, their effect will not be reflected in the shadows, causing a mismatch. 
  - Also applies to special transformations on the vertex shader, fluxuations in the number of polygons through Geometry Shader or Tessellation, etc.
  - Support for these cases would be difficult, and is unlikely in future updates. 

## License
[Unity Companion License](LICENSE.md) 
