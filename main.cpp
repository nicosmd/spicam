#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

// Camera and Encoder device paths
#define VIDEO_DEVICE "/dev/video0"     // USB camera
#define ENCODER_DEVICE "/dev/video11"  // H.264 encoder

int main() {
    // Step 1: Open the USB camera
    int cam_fd = open(VIDEO_DEVICE, O_RDWR);
    if (cam_fd == -1) {
        perror("Opening video device");
        return 1;
    }

    // Step 2: Set the camera format (YUYV in this case)
    struct v4l2_format cam_fmt;
    memset(&cam_fmt, 0, sizeof(cam_fmt));
    cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_fmt.fmt.pix.width = 640;
    cam_fmt.fmt.pix.height = 480;
    cam_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // Set YUYV (YUV422 format)
    cam_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(cam_fd, VIDIOC_S_FMT, &cam_fmt) == -1) {
        perror("Setting camera format");
        return 1;
    }

    // Step 3: Request camera buffers
    struct v4l2_requestbuffers cam_req;
    memset(&cam_req, 0, sizeof(cam_req));
    cam_req.count = 4;
    cam_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_req.memory = V4L2_MEMORY_USERPTR;  

    if (ioctl(cam_fd, VIDIOC_REQBUFS, &cam_req) == -1) {
        perror("Requesting camera buffer");
        return 1;
    }

    // Step 4: Query buffer and export DMABUF
    struct v4l2_buffer cam_buf;
    memset(&cam_buf, 0, sizeof(cam_buf));
    cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_buf.memory = V4L2_MEMORY_DMABUF;
    cam_buf.index = 0;

    if (ioctl(cam_fd, VIDIOC_QUERYBUF, &cam_buf) == -1) {
        perror("Querying camera buffer");
        return 1;
    }

    // Export DMABUF file descriptor for the camera buffer
    int dmabuf_fd;
    // cam_buf.flags = V4L2_BUF_FLAG_MAPPED;
    struct v4l2_exportbuffer expbuf;

    memset(&expbuf, 0 , sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = 0;
    if (ioctl(cam_fd, VIDIOC_EXPBUF, &expbuf) == -1) {
        perror("Exporting DMABUF");
        return 1;
    }

    dmabuf_fd = expbuf.fd;  // DMABUF file descriptor

    // Step 5: Start streaming from the camera
    if (ioctl(cam_fd, VIDIOC_STREAMON, &cam_buf.type) == -1) {
        perror("Camera stream on");
        return 1;
    }

    // Step 6: Open the H.264 encoder
    int enc_fd = open(ENCODER_DEVICE, O_RDWR);
    if (enc_fd == -1) {
        perror("Opening encoder device");
        return 1;
    }

    // Step 7: Set the encoder format (H.264 output)
    struct v4l2_format enc_fmt;
    memset(&enc_fmt, 0, sizeof(enc_fmt));
    enc_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    enc_fmt.fmt.pix.width = 640;
    enc_fmt.fmt.pix.height = 480;
    enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // Input format: YUYV
    enc_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(enc_fd, VIDIOC_S_FMT, &enc_fmt) == -1) {
        perror("Setting encoder input format");
        return 1;
    }

    enc_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;  // Output format: H.264

    if (ioctl(enc_fd, VIDIOC_S_FMT, &enc_fmt) == -1) {
        perror("Setting encoder output format");
        return 1;
    }

    // Step 8: Request encoder buffers
    struct v4l2_requestbuffers enc_req;
    memset(&enc_req, 0, sizeof(enc_req));
    enc_req.count = 1;
    enc_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    enc_req.memory = V4L2_MEMORY_DMABUF;  // Use DMABUF for memory

    if (ioctl(enc_fd, VIDIOC_REQBUFS, &enc_req) == -1) {
        perror("Requesting encoder buffers");
        return 1;
    }

    // Step 9: Capture and encode frames
    for (int i = 0; i < 100; i++) {
        // Dequeue camera buffer
        if (ioctl(cam_fd, VIDIOC_DQBUF, &cam_buf) == -1) {
            perror("Retrieving camera frame");
            return 1;
        }

        // Set the DMABUF file descriptor for the encoder
        struct v4l2_buffer enc_buf;
        memset(&enc_buf, 0, sizeof(enc_buf));
        enc_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        enc_buf.memory = V4L2_MEMORY_DMABUF;
        enc_buf.m.fd = dmabuf_fd;  // Pass the DMABUF file descriptor

        if (ioctl(enc_fd, VIDIOC_QBUF, &enc_buf) == -1) {
            perror("Queueing encoder buffer");
            return 1;
        }

        // Dequeue the encoder output (H.264 encoded frame)
        memset(&enc_buf, 0, sizeof(enc_buf));
        enc_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        enc_buf.memory = V4L2_MEMORY_DMABUF;

        if (ioctl(enc_fd, VIDIOC_DQBUF, &enc_buf) == -1) {
            perror("Retrieving encoded frame");
            return 1;
        }

        // Save the encoded frame (H.264)
        std::ofstream outfile("output.h264", std::ios::binary | std::ios::app);
        outfile.write((char*)dmabuf_fd, enc_buf.bytesused);  // Save encoded frame
        outfile.close();

        // Requeue the camera buffer for capturing the next frame
        if (ioctl(cam_fd, VIDIOC_QBUF, &cam_buf) == -1) {
            perror("Requeue camera buffer");
            return 1;
        }
    }

    // Step 10: Cleanup
    close(dmabuf_fd);  // Close the DMA-BUF file descriptor
    close(cam_fd);
    close(enc_fd);

    return 0;
}

