#include "v4l2_streamer.hpp"

int main() {
    V4L2Streamer streamer{"/dev/video0", 640, 480};

    streamer.start_streaming();

    for (int i = 0; i < 10; i++) {
        streamer.next_frame();
    }
}
