using NUnit.Framework;
using UTJ.RaytracedHardShadow;

namespace Unity.RaytracedHardShadow.Tests {
    public class RTHSAPITest
{
    [Test]
    public void RTHSInitialization()
    {
        rthsRenderer renderer = rthsRenderer.Create();
        renderer.Release();
    }
}

} // end namespace
