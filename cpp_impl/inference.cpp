#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    std::string pipeline =
        "udpsrc port=5000 caps=\"application/x-rtp, media=(string)video, encoding-name=(string)H264 \" ! "
        "rtph264depay ! avdec_h264 ! videoconvert ! appsink";


    std::cerr << "Starting the receiver pipeline" << std::endl;


    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open GStreamer pipeline!" << std::endl;
        std::cin.get();  // wait for ENTER
        return -1;
    }

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        cv::imshow("Local Stream", frame);
        if (cv::waitKey(1) == 27) break;  // ESC to quit
    }

    return 0;
}
