using UnityEngine;
using System;
using System.Runtime.InteropServices;
using System.Collections;
using System.Collections.Generic;

namespace FBCapture {
    using DestinationURL = String;
    using FBCAPTURE_HANDLE = IntPtr;

    [RequireComponent(typeof(Camera))]
    public class SurroundCapture : MonoBehaviour {

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS Create(FBCaptureConfig config, out FBCAPTURE_HANDLE handle);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS StartSession(FBCAPTURE_HANDLE handle, DestinationURL dstUrl);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS EncodeFrame(FBCAPTURE_HANDLE handle, IntPtr texturePtr);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS StopSession(FBCAPTURE_HANDLE handle);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS SaveScreenShot(FBCAPTURE_HANDLE handle, IntPtr texturePtr, DestinationURL dstUrl, bool flipTexture);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS Release(FBCAPTURE_HANDLE handle);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS GetSessionStatus(FBCAPTURE_HANDLE handle);

        [DllImport("FBCapture", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS Mute(FBCAPTURE_HANDLE handle, bool mute);

        #region CAPUTRE SDK

        public delegate void OnErrorCallback(ErrorType error, FBCAPTURE_STATUS? captureStatus);

        public event OnErrorCallback OnError = delegate { };

        #endregion

        public static SurroundCapture singleton;

        [Tooltip("Reference to camera that renders the scene")]
        public Camera sceneCamera;

        [Tooltip("Offset spherical coordinates (shift equirect)")]
        public Vector2 sphereOffset = Vector2.zero;
        [Tooltip("Scale spherical coordinates (flip equirect, usually just 1 or -1)")]
        public Vector2 sphereScale = Vector2.one;

        public enum EncodingFormat {
            Cubemap,
            Equirect,
        }

        [Header("Surround Capture Format")]
        public EncodingFormat encodingFormat = EncodingFormat.Equirect;

        [Header("Cubemap Size")]
        public int cubemapSize = 1024;

        private RenderTexture cubemapTex;
        private RenderTexture outputTex;  // equirect or cubemap ends up here
        private RenderTexture externalTex;  // actual texture pointer passed to capture encoder

        [Header("Surround Capture Materials")]
        public Material convertMaterial;
        public Material outputCubemapMaterial;
        public Material downSampleMaterial;

        private volatile bool stopSessionRequested;
        private volatile bool sessionActive;

        private bool stopSessionCalled;

        private FBCAPTURE_HANDLE handle;

        private DestinationURL destinationUrl;

        // Rotate camera for cubemap lookup
        private bool includeCameraRotation = false;

        private float fpsTimer = 0f;
        private int fps = 30;
        private int numFrames = 0;

        void Awake() {
#if (UNITY_ANDROID && !UNITY_EDITOR) || UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
        Destroy(gameObject);
        return;
#endif
            if (singleton != null) {
                DestroyImmediate(gameObject);
                return;
            }
            singleton = this;
        }

        void Start() {
            // create cubemap render texture
            cubemapTex = new RenderTexture(cubemapSize, cubemapSize, 0);
#if UNITY_5_4_OR_NEWER
            cubemapTex.dimension = UnityEngine.Rendering.TextureDimension.Cube;
#else
        cubemapTex.isCubemap = true;
#endif
            cubemapTex.hideFlags = HideFlags.HideAndDontSave;
            cubemapTex.Create();

            ResetStatus();
        }

        void Update() {
            if (!sessionActive) {
                // Debug.LogFormat("[LOG] fps: {0}", 1.0f / Time.deltaTime);
                return;
            }

            FBCAPTURE_STATUS status = FBCAPTURE_STATUS.OK;
            if (stopSessionCalled) {
                status = GetSessionStatus(handle);
                if (status == FBCAPTURE_STATUS.OK) {
                    Debug.LogFormat("[SurroundCapture] Finalized capture. URL: {0}", destinationUrl);
                    ResetStatus();
                } else {
                    Debug.LogFormat("[ERROR] FBCaptureStopSession() failed. Session status: {0}", status);
                    onFailure(ErrorType.STOP_SESSION_FAIL, status);
                }
                return;
            }

            if (stopSessionRequested) {
                status = StopSession(handle);
                if (status == FBCAPTURE_STATUS.OK) {
                    Debug.Log("[SurroundCapture] Stopping session.");
                    stopSessionCalled = true;
                } else {
                    Debug.LogFormat("[ERROR] FBCaptureStopSession() failed. Session status: {0}", status);
                    onFailure(ErrorType.STOP_SESSION_FAIL, status);
                }
                return;
            }

            fpsTimer += Time.deltaTime;
            Debug.LogFormat("[LOG] fps: {0}", 1.0f / Time.deltaTime);
            if (fpsTimer < (1.0f / fps))
                return;

            // rebase the fps timer
            fpsTimer -= (1.0f / fps);

            if (sceneCamera) {
                sceneCamera.transform.position = transform.position;
                sceneCamera.RenderToCubemap(cubemapTex);  // render cubemap
            }

            status = EncodeFrame(handle, externalTex.GetNativeTexturePtr());
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[ERROR] FBCaptureEncodeFrame() failed. Session status: {0}", status);
                onFailure(ErrorType.ENCODE_FRAME_FAILED, status);
            } else
                numFrames++;
        }

        public void Initialize(
            int width,
            int height,
            int fps,
            int bitrate,
            int gop
         ) {
            FBCaptureConfig config = new FBCaptureConfig(bitrate, fps, gop);
            FBCAPTURE_STATUS status = Create(config, out handle);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[ERROR] FBCapture::Create() failed. Session status: {0}", status);
                onFailure(ErrorType.INITIALIZE_FAILED, status);
                return;
            }

            this.fps = fps;

            // create render texture for 360 capture
            SetOutputSize(width, height);

            Debug.LogFormat("[SurroundCapture] FBCapture initialized. (bitrate: {0}, fps{1}, gop:{2})", bitrate, fps, gop);
        }

        public void StartEncoding(DestinationURL videoUrl) {
            FBCAPTURE_STATUS status = StartSession(handle, videoUrl);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[Error] FBCapture::StartSession() failed. Session status: {0}", status);
                onFailure(ErrorType.START_SESSION_FAILED, status);
                return;
            }

            sessionActive = true;
            fpsTimer = 0.0f;
            numFrames = 0;

            Debug.LogFormat("[SurroundCapture] FBCapture session started. URL: {0}", videoUrl);
        }

        public void StopEncoding() {
            if (!sessionActive || stopSessionRequested || stopSessionCalled)
                return;
            stopSessionRequested = true;
        }

        public void TakeScreenshot(DestinationURL screenshotUrl) {
            destinationUrl = screenshotUrl;
            StartCoroutine(CaptureScreenshot());
        }

        IEnumerator CaptureScreenshot() {
            // yield a frame to re-render into the rendertexture
            yield return new WaitForEndOfFrame();

            FBCAPTURE_STATUS status = SaveScreenShot(handle, externalTex.GetNativeTexturePtr(), destinationUrl, true);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[ERROR] FBCapture::SaveScreenShot failed. Session status: {0}", status);
                OnError(ErrorType.SCREENSHOT_FAILED, status);
            } else {
                Debug.LogFormat("[SurroundCapture] Saving screenshot: {0}", destinationUrl);
            }
        }

        public void Release() {
            FBCAPTURE_STATUS status = Release(handle);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[Error] FBCapture::Release() failed. Session status: {0}", status);
                onFailure(ErrorType.RELEASE_FAILED, status);
            }
        }

        void RenderCubeFace(CubemapFace face, float x, float y, float w, float h) {
            // texture coordinates for displaying each cube map face
            Vector3[] faceTexCoords = {
                // +x
                new Vector3(1, 1, 1),
                new Vector3(1, -1, 1),
                new Vector3(1, -1, -1),
                new Vector3(1, 1, -1),
                // -x
                new Vector3(-1, 1, -1),
                new Vector3(-1, -1, -1),
                new Vector3(-1, -1, 1),
                new Vector3(-1, 1, 1),

                // -y
                new Vector3(-1, -1, 1),
                new Vector3(-1, -1, -1),
                new Vector3(1, -1, -1),
                new Vector3(1, -1, 1),
                // +y // flipped with -y for FB live
                new Vector3(-1, 1, -1),
                new Vector3(-1, 1, 1),
                new Vector3(1, 1, 1),
                new Vector3(1, 1, -1),

                // +z
                new Vector3(-1, 1, 1),
                new Vector3(-1, -1, 1),
                new Vector3(1, -1, 1),
                new Vector3(1, 1, 1),
                // -z
                new Vector3(1, 1, -1),
                new Vector3(1, -1, -1),
                new Vector3(-1, -1, -1),
                new Vector3(-1, 1, -1),
            };

            GL.PushMatrix();
            GL.LoadOrtho();
            GL.LoadIdentity();

            int i = (int)face;

            GL.Begin(GL.QUADS);
            GL.TexCoord(faceTexCoords[i * 4]);
            GL.Vertex3(x, y, 0);
            GL.TexCoord(faceTexCoords[i * 4 + 1]);
            GL.Vertex3(x, y + h, 0);
            GL.TexCoord(faceTexCoords[i * 4 + 2]);
            GL.Vertex3(x + w, y + h, 0);
            GL.TexCoord(faceTexCoords[i * 4 + 3]);
            GL.Vertex3(x + w, y, 0);
            GL.End();

            GL.PopMatrix();
        }

        void SetMaterialParameters(Material material) {
            // convert to equirectangular
            material.SetTexture("_CubeTex", cubemapTex);
            material.SetVector("_SphereScale", sphereScale);
            material.SetVector("_SphereOffset", sphereOffset);

            // cubemap rendered along axes, so we do rotation by rotating the cubemap lookup
            if (includeCameraRotation) {
                material.SetMatrix("_CubeTransform", Matrix4x4.TRS(Vector3.zero, transform.rotation, Vector3.one));
            } else {
                material.SetMatrix("_CubeTransform", Matrix4x4.identity);
            }
        }

        void DisplayCubeMap() {
            SetMaterialParameters(outputCubemapMaterial);
            outputCubemapMaterial.SetPass(0);

            Graphics.SetRenderTarget(outputTex);

            float s = 1.0f / 3.0f;
            RenderCubeFace(CubemapFace.PositiveX, 0.0f, 0.5f, s, 0.5f);
            RenderCubeFace(CubemapFace.NegativeX, s, 0.5f, s, 0.5f);
            RenderCubeFace(CubemapFace.PositiveY, s * 2.0f, 0.5f, s, 0.5f);

            RenderCubeFace(CubemapFace.NegativeY, 0.0f, 0.0f, s, 0.5f);
            RenderCubeFace(CubemapFace.PositiveZ, s, 0.0f, s, 0.5f);
            RenderCubeFace(CubemapFace.NegativeZ, s * 2.0f, 0.0f, s, 0.5f);

            Graphics.SetRenderTarget(null);
            Graphics.Blit(outputTex, externalTex);
            Debug.Log("DisplayCubeMap");
        }

        void DisplayEquirect() {
            SetMaterialParameters(convertMaterial);
            Graphics.Blit(null, externalTex, convertMaterial);
        }

        void SetOutputSize(int width, int height) {
            if (outputTex != null)
                Destroy(outputTex);

            outputTex = new RenderTexture(width, height, 0);
            outputTex.hideFlags = HideFlags.HideAndDontSave;

            if (externalTex != null)
                Destroy(externalTex);

            externalTex = new RenderTexture(width, height, 0);
            externalTex.hideFlags = HideFlags.HideAndDontSave;
        }

        private void onFailure(ErrorType errorType, FBCAPTURE_STATUS status) {
            Release();
            ResetStatus();
            OnError(errorType, status);
        }

        private void ResetStatus() {
            sessionActive = false;
            stopSessionRequested = false;
            stopSessionCalled = false;
        }

        void OnPreRender() {
            if (encodingFormat == EncodingFormat.Cubemap)
                DisplayCubeMap();
            else
                DisplayEquirect();
        }

        void OnApplicationQuit() {
            Release();
            if (cubemapTex)
                Destroy(cubemapTex);
            if (outputTex)
                Destroy(outputTex);
            if (externalTex)
                Destroy(externalTex);
        }

        void OnDestroy() {
            Release();
            if (cubemapTex)
                Destroy(cubemapTex);
            if (outputTex)
                Destroy(outputTex);
            if (externalTex)
                Destroy(externalTex);
        }
    }
}
