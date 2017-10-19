using UnityEngine;
using System.Collections;
using System.IO;
using System;


namespace FBCapture {
    public class CaptureOption : MonoBehaviour {

        [Header("Capture Hotkeys")]
        public KeyCode CaptureInitKey = KeyCode.F1;
        public KeyCode CaptureScreenShotKey = KeyCode.F2;
        public KeyCode CaptureVideoStartKey = KeyCode.F3;
        public KeyCode CaptureVideoStopKey = KeyCode.F4;
        public KeyCode CaptureLiveStartKey = KeyCode.F5;
        public KeyCode CaptureLiveStopKey = KeyCode.F6;
        public KeyCode CaptureRleaseKey = KeyCode.F7;

        [Header("Encoding Options")]
        public int Width = 4096;
        public int Height = 2048;
        public int Fps = 30;
        public int Bitrate = 5000000;
        public int Gop = 30;

        [Header("Output Path")]
        public string VideoPathURL = GetDefaultOutputPath(".mp4");
        public string ScreenshotPathURL = GetDefaultOutputPath(".jpg");
        public string LiveStreamingURL;
        public string LiveStreamingKey;

        private SurroundCapture surroundCapture;

        private int fps_;
        private int bitrate_;
        private int gop_;
        private int width_;
        private int height_;

        private string videoUrl_;
        private string screenshotUrl_;
        private string streamUrl_;

        void Start() {
            surroundCapture = GetComponent<SurroundCapture>();
            surroundCapture.enabled = true;
        }

        void Update() {
            if (!keyPressed())
                return;

            if (Input.GetKeyDown(CaptureInitKey)) {
                onOptionSet();
                surroundCapture.Initialize(width_, height_, fps_, bitrate_, gop_);
            } else if (Input.GetKeyDown(CaptureScreenShotKey))
                surroundCapture.TakeScreenshot(screenshotUrl_);
            else if (Input.GetKeyDown(CaptureVideoStartKey))
                surroundCapture.StartEncoding(videoUrl_);
            else if (Input.GetKeyDown(CaptureVideoStopKey))
                surroundCapture.StopEncoding();
            else if (Input.GetKeyDown(CaptureLiveStartKey))
                surroundCapture.StartEncoding(streamUrl_);
            else if (Input.GetKeyDown(CaptureLiveStopKey))
                surroundCapture.StopEncoding();
            else if (Input.GetKeyDown(CaptureRleaseKey))
                surroundCapture.Release();
        }

        private string StreamServerURL() {
            if (LiveStreamingURL.EndsWith("/")) {
                LiveStreamingURL = LiveStreamingURL.Remove(LiveStreamingURL.Length - 1);
            }
            return LiveStreamingURL + '/' + LiveStreamingKey;
        }

        public static string GetDefaultOutputPath(string ext) {
            string defaultFolder = System.IO.Path.Combine(Directory.GetCurrentDirectory(), "Gallery");
            // create the directory
            if (!Directory.Exists(defaultFolder))
                Directory.CreateDirectory(defaultFolder);
            DateTime dt = DateTime.Now;
            string filename = dt.ToString("yyyy-MM-dd_HH-mm-ss") + ext;
            return System.IO.Path.Combine(defaultFolder, "FBCapture_" + filename);
        }

        private void onOptionSet() {
            width_ = Width;
            height_ = Height;
            fps_ = Fps;
            bitrate_ = Bitrate;
            gop_ = Gop;

            videoUrl_ = VideoPathURL;
            screenshotUrl_ = ScreenshotPathURL;
            streamUrl_ = StreamServerURL();
        }

        private bool keyPressed() {
            return Input.GetKeyDown(CaptureInitKey) ||
                Input.GetKeyDown(CaptureScreenShotKey) ||
                Input.GetKeyDown(CaptureVideoStartKey) ||
                Input.GetKeyDown(CaptureVideoStopKey) ||
                Input.GetKeyDown(CaptureLiveStartKey) ||
                Input.GetKeyDown(CaptureLiveStopKey) ||
                Input.GetKeyDown(CaptureRleaseKey);
        }
    }
}
