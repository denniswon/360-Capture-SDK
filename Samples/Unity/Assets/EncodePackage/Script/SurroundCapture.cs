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

        [DllImport("FBCapture", EntryPoint = "Create", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureCreate(FBCaptureConfig config, out FBCAPTURE_HANDLE handle_);

        [DllImport("FBCapture", EntryPoint = "StartSession", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureStartSession(FBCAPTURE_HANDLE handle_, DestinationURL dstUrl);

        [DllImport("FBCapture", EntryPoint = "EncodeFrame", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureEncodeFrame(FBCAPTURE_HANDLE handle_, IntPtr texturePtr);

        [DllImport("FBCapture", EntryPoint = "StopSession", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureStopSession(FBCAPTURE_HANDLE handle_);

        [DllImport("FBCapture", EntryPoint = "SaveScreenShot", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureSaveScreenShot(FBCAPTURE_HANDLE handle_, IntPtr texturePtr, DestinationURL dstUrl, bool flipTexture);

        [DllImport("FBCapture", EntryPoint = "Release", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureRelease(FBCAPTURE_HANDLE handle_);

        [DllImport("FBCapture", EntryPoint = "GetSessionStatus", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureGetSessionStatus(FBCAPTURE_HANDLE handle_);

        [DllImport("FBCapture", EntryPoint = "Mute", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        private static extern FBCAPTURE_STATUS FBCaptureMute(FBCAPTURE_HANDLE handle_, bool mute);

        #region CAPUTRE SDK

        public delegate void OnErrorCallback(ErrorType error, FBCAPTURE_STATUS? captureStatus);

        public event OnErrorCallback OnError = delegate { };

        #endregion

        public static SurroundCapture sSurroundCapture;

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

        private RenderTexture cubemapTex_;
        private RenderTexture outputTex_;  // equirect or cubemap ends up here
        private RenderTexture externalTex_;  // actual texture pointer passed to capture encoder

        [Header("Surround Capture Materials")]
        public Material convertMaterial;
        public Material outputCubemapMaterial;
        public Material downSampleMaterial;

        private volatile bool stopSessionRequested_;
        private volatile bool sessionActive_;

        private bool stopSessionCalled_;

        private FBCAPTURE_HANDLE handle_ = IntPtr.Zero;

        private DestinationURL destinationUrl_;

        // Rotate camera for cubemap lookup
        private bool includeCameraRotation_ = false;

        private float fps_Timer_ = 0f;
        private int fps_ = 30;
        private int numFrames_ = 0;

        void Awake() {
#if (UNITY_ANDROID && !UNITY_EDITOR) || UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
        Destroy(gameObject);
        return;
#endif
            if (sSurroundCapture != null) {
                DestroyImmediate(gameObject);
                return;
            }
            sSurroundCapture = this;
        }

        void Start() {
            // create cubemap render texture
            cubemapTex_ = new RenderTexture(cubemapSize, cubemapSize, 0);
#if UNITY_5_4_OR_NEWER
            cubemapTex_.dimension = UnityEngine.Rendering.TextureDimension.Cube;
#else
        cubemapTex_.isCubemap = true;
#endif
            cubemapTex_.hideFlags = HideFlags.HideAndDontSave;
            cubemapTex_.Create();

            ResetStatus();
        }

        void Update() {
            if (!sessionActive_) {
                Debug.LogFormat("[LOG] fps_: {0}", 1.0f / Time.deltaTime);
                return;
            }

            FBCAPTURE_STATUS status = FBCAPTURE_STATUS.OK;
            if (stopSessionCalled_) {
                status = FBCaptureGetSessionStatus(handle_);
                if (status == FBCAPTURE_STATUS.OK) {
                    Debug.LogFormat("[SurroundCapture] Finalized capture. URL: {0}", destinationUrl_);
                    ResetStatus();
                } else if (status != FBCAPTURE_STATUS.SESSION_ACTIVE) {
                    Debug.LogFormat("[ERROR] FBCaptureStopSession() failed. Session status: {0}", status);
                    OnFailure(ErrorType.STOP_SESSION_FAIL, status);
                }
                return;
            }

            if (stopSessionRequested_) {
                status = FBCaptureStopSession(handle_);
                if (status == FBCAPTURE_STATUS.OK) {
                    Debug.Log("[SurroundCapture] Stopping session.");
                    stopSessionCalled_ = true;
                } else {
                    Debug.LogFormat("[ERROR] FBCaptureStopSession() failed. Session status: {0}", status);
                    OnFailure(ErrorType.STOP_SESSION_FAIL, status);
                }
                return;
            }

            fps_Timer_ += Time.deltaTime;
            Debug.LogFormat("[LOG] fps_: {0}", 1.0f / Time.deltaTime);
            if (fps_Timer_ < (1.0f / fps_))
                return;

            // rebase the fps_ timer
            fps_Timer_ -= (1.0f / fps_);

            if (sceneCamera) {
                sceneCamera.transform.position = transform.position;
                sceneCamera.RenderToCubemap(cubemapTex_);  // render cubemap
            }

            status = FBCaptureEncodeFrame(handle_, externalTex_.GetNativeTexturePtr());
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[ERROR] FBCaptureEncodeFrame() failed. Session status: {0}", status);
                OnFailure(ErrorType.ENCODE_FRAME_FAILED, status);
            } else
                numFrames_++;
        }

        public void Initialize(
            int width,
            int height,
            int fps_,
            int bitrate,
            int gop
         ) {
            FBCaptureConfig config = new FBCaptureConfig(bitrate, fps_, gop);
            FBCAPTURE_STATUS status = FBCaptureCreate(config, out handle_);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[ERROR] FBCaptureCreate() failed. Session status: {0}", status);
                OnFailure(ErrorType.INITIALIZE_FAILED, status);
                return;
            }

            this.fps_ = fps_;

            // create render texture for 360 capture
            SetOutputSize(width, height);

            Debug.LogFormat("[SurroundCapture] FBCapture initialized. (bitrate: {0}, fps_{1}, gop:{2})", bitrate, fps_, gop);
        }

        public void StartEncoding(DestinationURL videoUrl) {
            FBCAPTURE_STATUS status = FBCaptureStartSession(handle_, videoUrl);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[Error] FBCaptureStartSession() failed. Session status: {0}", status);
                OnFailure(ErrorType.START_SESSION_FAILED, status);
                return;
            }

            sessionActive_ = true;
            fps_Timer_ = 0.0f;
            numFrames_ = 0;

            Debug.LogFormat("[SurroundCapture] FBCapture session started. URL: {0}", videoUrl);
        }

        public void StopEncoding() {
            if (!sessionActive_ || stopSessionRequested_ || stopSessionCalled_)
                return;
            stopSessionRequested_ = true;
        }

        public void TakeScreenshot(DestinationURL screenshotUrl) {
            destinationUrl_ = screenshotUrl;
            StartCoroutine(CaptureScreenshot());
        }

        IEnumerator CaptureScreenshot() {
            // yield a frame to re-render into the rendertexture
            yield return new WaitForEndOfFrame();

            FBCAPTURE_STATUS status = FBCaptureSaveScreenShot(handle_, externalTex_.GetNativeTexturePtr(), destinationUrl_, true);
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[ERROR] FBCaptureSaveScreenShot failed. Session status: {0}", status);
                OnError(ErrorType.SCREENSHOT_FAILED, status);
            } else {
                Debug.LogFormat("[SurroundCapture] Saving screenshot: {0}", destinationUrl_);
            }
        }

        public void Release() {
            if (handle_ == IntPtr.Zero) {
                return;
            }

            FBCAPTURE_STATUS status = FBCaptureRelease(handle_);
            handle_ = IntPtr.Zero;
            if (status != FBCAPTURE_STATUS.OK) {
                Debug.LogFormat("[Error] FBCaptureRelease() failed. Session status: {0}", status);
                OnFailure(ErrorType.RELEASE_FAILED, status);
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
            material.SetTexture("_CubeTex", cubemapTex_);
            material.SetVector("_SphereScale", sphereScale);
            material.SetVector("_SphereOffset", sphereOffset);

            // cubemap rendered along axes, so we do rotation by rotating the cubemap lookup
            if (includeCameraRotation_) {
                material.SetMatrix("_CubeTransform", Matrix4x4.TRS(Vector3.zero, transform.rotation, Vector3.one));
            } else {
                material.SetMatrix("_CubeTransform", Matrix4x4.identity);
            }
        }

        void DisplayCubeMap() {
            SetMaterialParameters(outputCubemapMaterial);
            outputCubemapMaterial.SetPass(0);

            Graphics.SetRenderTarget(outputTex_);

            float s = 1.0f / 3.0f;
            RenderCubeFace(CubemapFace.PositiveX, 0.0f, 0.5f, s, 0.5f);
            RenderCubeFace(CubemapFace.NegativeX, s, 0.5f, s, 0.5f);
            RenderCubeFace(CubemapFace.PositiveY, s * 2.0f, 0.5f, s, 0.5f);

            RenderCubeFace(CubemapFace.NegativeY, 0.0f, 0.0f, s, 0.5f);
            RenderCubeFace(CubemapFace.PositiveZ, s, 0.0f, s, 0.5f);
            RenderCubeFace(CubemapFace.NegativeZ, s * 2.0f, 0.0f, s, 0.5f);

            Graphics.SetRenderTarget(null);
            Graphics.Blit(outputTex_, externalTex_);
            Debug.Log("DisplayCubeMap");
        }

        void DisplayEquirect() {
            SetMaterialParameters(convertMaterial);
            Graphics.Blit(null, externalTex_, convertMaterial);
        }

        void SetOutputSize(int width, int height) {
            if (outputTex_ != null)
                Destroy(outputTex_);

            outputTex_ = new RenderTexture(width, height, 0);
            outputTex_.hideFlags = HideFlags.HideAndDontSave;

            if (externalTex_ != null)
                Destroy(externalTex_);

            externalTex_ = new RenderTexture(width, height, 0);
            externalTex_.hideFlags = HideFlags.HideAndDontSave;
        }

        private void OnFailure(ErrorType errorType, FBCAPTURE_STATUS status) {
            Release();
            ResetStatus();
            OnError(errorType, status);
        }

        private void ResetStatus() {
            sessionActive_ = false;
            stopSessionRequested_ = false;
            stopSessionCalled_ = false;
        }

        void OnPreRender() {
            if (encodingFormat == EncodingFormat.Cubemap)
                DisplayCubeMap();
            else
                DisplayEquirect();
        }

        void OnApplicationQuit() {
            Release();
            if (cubemapTex_)
                Destroy(cubemapTex_);
            if (outputTex_)
                Destroy(outputTex_);
            if (externalTex_)
                Destroy(externalTex_);
        }

        void OnDestroy() {
            Release();
            if (cubemapTex_)
                Destroy(cubemapTex_);
            if (outputTex_)
                Destroy(outputTex_);
            if (externalTex_)
                Destroy(externalTex_);
        }
    }
}
