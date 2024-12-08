//
// Copyright (c) 2024 Nico Schmidt
//

#include <memory>

#include "requeing_package.hpp"
#include "v4l2_streamer.hpp"

int main() {
    V4L2Streamer streamer{"/dev/video0", 640, 480};

    streamer.start_streaming();

    for (int i = 0; i < 10; i++) {
        streamer.next_frame();
    }

    std::make_unique<DmaBuf>(0,0,"test");

}
