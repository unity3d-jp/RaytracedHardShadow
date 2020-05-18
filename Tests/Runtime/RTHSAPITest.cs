using NUnit.Framework;

namespace Unity.RaytracedHardShadow.Tests {
internal class RTHSAPITest {
    [Test]
    public void RTHSInitialization() {
        rthsRenderer renderer = rthsRenderer.Create();
        renderer.Release();
    }
}

} // end namespace
