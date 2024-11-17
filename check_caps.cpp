//
// Copyright (c) 2024 Nico Schmidt
//

#include <cstdio>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

int main(int argc, char** argv) {

    if(argc < 3) {
        std::cout << "Not enough arguments" << std::endl;
        return 1;
    }

    std::string device_file{argv[1]};
    std::string type{argv[2]};

    v4l2_buf_type buf_type{};

    if(type == "output") {
        std::cout << "use buftype output" << std::endl;
        buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    }
    else if (type == "capture") {
        std::cout << "use buftype capture" << std::endl;
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    else if (type == "output_mplane") {
        std::cout << "use buftype output mplane" << std::endl;
        buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    }
    else if (type == "capture_mplane") {
        std::cout << "use buftype capture mplane" << std::endl;
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }
    else {
        std::cout << "Invalid buf type" << std::endl;
        return 1;
    }
    
    int device_fd = open(device_file.c_str(), O_RDWR);

    if(device_fd == -1) {
        perror("Opening video device");
        return 1;
    }

    struct v4l2_capability caps;

    if(ioctl(device_fd, VIDIOC_QUERYCAP, &caps)) {
        perror("Query Capabilites");
    }
    

    if(caps.capabilities & V4L2_CAP_STREAMING) {
        std::cout << "streaming supported!" << std::endl;
    }


    struct v4l2_requestbuffers test_userptr_req;
    memset(&test_userptr_req, 0, sizeof(test_userptr_req));
    test_userptr_req.count = 4;
    test_userptr_req.type = buf_type;
    test_userptr_req.memory = V4L2_MEMORY_USERPTR;  

    if (ioctl(device_fd, VIDIOC_REQBUFS, &test_userptr_req) == -1) {
        perror("Test userptr Request");
    }
    else {
      std::cout << "Userptr supported!" << std::endl;
    }

    struct v4l2_requestbuffers test_mmap_req;
    memset(&test_userptr_req, 0, sizeof(test_mmap_req));
    test_mmap_req.count = 4;
    test_mmap_req.type = buf_type;
    test_mmap_req.memory = V4L2_MEMORY_MMAP;  

    if (ioctl(device_fd, VIDIOC_REQBUFS, &test_mmap_req) == -1) {
        perror("Test mmap request");
    }
    else {
      std::cout << "mmap supported!" << std::endl;
    }

    struct v4l2_requestbuffers test_dmabuf_req;
    memset(&test_userptr_req, 0, sizeof(test_dmabuf_req));
    test_dmabuf_req.count = 4;
    test_dmabuf_req.type = buf_type;
    test_dmabuf_req.memory = V4L2_MEMORY_DMABUF;  

    if (ioctl(device_fd, VIDIOC_REQBUFS, &test_dmabuf_req) == -1) {
        perror("Test dmabuf request");
    }
    else {
      std::cout << "dmabuf supported!" << std::endl;
    }

    if(test_dmabuf_req.capabilities & V4L2_BUF_CAP_SUPPORTS_DMABUF) {
        std::cout << "dmabuf really supported" << std::endl;
    }
    close(device_fd);
    
}
