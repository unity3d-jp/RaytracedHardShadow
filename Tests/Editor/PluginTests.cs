using NUnit.Framework;
using UnityEngine.Windows;

namespace Unity.RaytracedHardShadow.EditorTests {

internal class PluginTests {

    [Test]
    public void CheckPluginExist() {
        const string PATH = "Packages/com.unity.raytracedhardshadow/Runtime/Plugins/x86_64/rths.dll";
        Assert.True(File.Exists(PATH));
    }
   
}
}
