
#include <cstdint>
#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "dmabuf.h"

// Camera and Encoder device paths
#define VIDEO_DEVICE "/dev/video0"    // USB camera
#define ENCODER_DEVICE "/dev/video11" // H.264 encoder

class Yield {
private:
  std::function<void()> m_yield_cb;

public:
  Yield(std::function<void()> cb) : m_yield_cb(std::move(cb)) {}

  ~Yield() noexcept { m_yield_cb(); }
};

int main() {
  constexpr std::uint8_t num_buf{8};

  int cam_fd = open(VIDEO_DEVICE, O_RDWR);
  if (cam_fd == -1) {
    perror("Opening video device");
    return 1;
  }

  Yield cam_yield([&cam_fd]() { close(cam_fd); });

  struct v4l2_format cam_fmt;
  memset(&cam_fmt, 0, sizeof(cam_fmt));
  cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  cam_fmt.fmt.pix.width = 640;
  cam_fmt.fmt.pix.height = 480;
  cam_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // Set YUYV (YUV422 format)
  cam_fmt.fmt.pix.field = V4L2_FIELD_ANY;

  if (ioctl(cam_fd, VIDIOC_S_FMT, &cam_fmt) == -1) {
    perror("Setting camera format");
    return 1;
  }

  std::cout << "Actual image width:  " << cam_fmt.fmt.pix.width << std::endl;
  std::cout << "Actual image height: " << cam_fmt.fmt.pix.height << std::endl;
  std::cout << "Actual image format: " << (char *)&cam_fmt.fmt.pix.pixelformat
            << std::endl;
  std::cout << "Actual image size: " << cam_fmt.fmt.pix.sizeimage << std::endl;

  struct v4l2_requestbuffers cam_req;
  memset(&cam_req, 0, sizeof(cam_req));
  cam_req.count = num_buf;
  cam_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  cam_req.memory = V4L2_MEMORY_DMABUF;

  if (ioctl(cam_fd, VIDIOC_REQBUFS, &cam_req) == -1) {
    perror("Requesting camera buffer");
    return 1;
  }

  auto dmabuf_heap_fd = dmabuf_heap_open();

  if (dmabuf_heap_fd < 0) {
    printf("Could not open dmabuf-heap\n");
    return 1;
  }

  Yield dmabuf_yield([&dmabuf_heap_fd]() { dmabuf_heap_open(); });

  int dmabufs_fd[num_buf];
  void *dmabuf_maps[num_buf];

  for (int i = 0; i < num_buf; i++) {
    std::cout << "buffer size: " << cam_fmt.fmt.pix.sizeimage << std::endl;
    dmabufs_fd[i] =
        dmabuf_heap_alloc(dmabuf_heap_fd, NULL, cam_fmt.fmt.pix.sizeimage);
    if (dmabufs_fd[i] < 0) {
      printf("Failed to allocate dmabuf");
      return 1;
    }

    dmabuf_maps[i] = mmap(0, cam_fmt.fmt.pix.sizeimage, PROT_WRITE | PROT_READ,
                          MAP_SHARED, dmabufs_fd[i], 0);
    if (dmabuf_maps[i] == MAP_FAILED) {
      printf("Failed to map dmabuf %d\n", i);
      return 1;
    }
  }

  /* enque dmabufs into v4l2 device */
  for (int i = 0; i < num_buf; i++) {
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.index = i;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.m.fd = dmabufs_fd[i];

    if (ioctl(cam_fd, VIDIOC_QBUF, &buf)) {
      perror("Queueing DMABUF");
      return 1;
    }
  }

  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(cam_fd, VIDIOC_STREAMON, &type)) {
    perror("Start streaming");

    return 1;
  }

  struct pollfd pfds[1];

  pfds[0].fd = cam_fd;
  pfds[0].events = POLLIN;

  int loop_count = 0;
  struct v4l2_buffer buf;
  int buf_index;

  /* dequeue a buffer */
  memset(&buf, 0, sizeof(buf));
  buf.memory = V4L2_MEMORY_DMABUF;
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  std::cout << "try to poll" << std::endl;
  if (ioctl(cam_fd, VIDIOC_DQBUF, &buf)) {
    printf("VIDIOC_DQBUF: %s\n", strerror(errno));
  }

  buf_index = buf.index;

  std::cout << "Got an image! Buffer index: " << buf_index << std::endl;

  // Step 6: Open the H.264 encoder
  int enc_fd = open(ENCODER_DEVICE, O_RDWR);
  if (enc_fd == -1) {
    perror("Opening encoder device");
    return 1;
  }

  // Step 7: Set the encoder format (H.264 output)
  struct v4l2_format enc_fmt;
  memset(&enc_fmt, 0, sizeof(enc_fmt));
  enc_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  enc_fmt.fmt.pix.width = 640;
  enc_fmt.fmt.pix.height = 480;
  enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // Input format: YUYV
  enc_fmt.fmt.pix.field = V4L2_FIELD_ANY;

  if (ioctl(enc_fd, VIDIOC_S_FMT, &enc_fmt) == -1) {
    perror("Setting encoder input format");
    return 1;
  }

  enc_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264; // Output format: H.264

  if (ioctl(enc_fd, VIDIOC_S_FMT, &enc_fmt) == -1) {
    perror("Setting encoder output format");
    return 1;
  }

  // Step 8: Request encoder buffers
  struct v4l2_requestbuffers enc_req;
  memset(&enc_req, 0, sizeof(enc_req));
  enc_req.count = 1;
  enc_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  enc_req.memory = V4L2_MEMORY_DMABUF; // Use DMABUF for memory

  if (ioctl(enc_fd, VIDIOC_REQBUFS, &enc_req) == -1) {
    perror("Requesting encoder buffers");
    return 1;
  }

  memset(&enc_req, 0, sizeof(enc_req));
  enc_req.count = 1;
  enc_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  enc_req.memory = V4L2_MEMORY_MMAP; // Use DMABUF for memory

  if (ioctl(enc_fd, VIDIOC_REQBUFS, &enc_req) == -1) {
    perror("Requesting encoder buffers");
    return 1;
  }

  struct v4l2_buffer enc_result_buffer;
  struct v4l2_plane enc_planes;

  memset(&enc_result_buffer, 0, sizeof(enc_result_buffer));
  enc_result_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  enc_result_buffer.memory = V4L2_MEMORY_MMAP;
  memset(&enc_planes, 0, sizeof(enc_planes));
  enc_result_buffer.m.planes = &enc_planes;
  enc_result_buffer.length = 1;
  enc_result_buffer.index = 0;
  enc_result_buffer.reserved2 = 0;
  enc_result_buffer.reserved = 0;

  if (ioctl(enc_fd, VIDIOC_QUERYBUF, &enc_result_buffer) == -1) {
    perror("Querying buffer");
    return 1;
  }

  if (ioctl(enc_fd, VIDIOC_QBUF, &enc_result_buffer) == -1) {
    perror("Queueing buffer");
    return 1;
  }

  std::cout << "enc_result_buffer.length: "
            << enc_result_buffer.m.planes[0].length << std::endl;
  std::cout << "enc_result_buffer.data_offset: "
            << enc_result_buffer.m.planes[0].m.mem_offset << std::endl;

  // Memory mapping
  void *enc_buffer =
      mmap(NULL, enc_result_buffer.m.planes[0].length, PROT_READ | PROT_WRITE,
           MAP_SHARED, enc_fd, enc_result_buffer.m.planes[0].m.mem_offset);
  if (enc_buffer == MAP_FAILED) {
    perror("mmap");
  }

  struct v4l2_plane planes;

  struct v4l2_buffer enc_buf;
  memset(&enc_buf, 0, sizeof(enc_buf));
  enc_buf.index = 0;
  enc_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  enc_buf.memory = V4L2_MEMORY_DMABUF;
  memset(&planes, 0, sizeof(planes));
  enc_buf.m.planes = &planes;
  enc_buf.length = 1;
  enc_buf.m.planes[0].m.fd =
      dmabufs_fd[buf_index]; // Pass the DMABUF file descriptor

  if (ioctl(enc_fd, VIDIOC_QBUF, &enc_buf) == -1) {
    perror("Queueing encoder buffer");
    return 1;
  }

  int enc_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  if (ioctl(enc_fd, VIDIOC_STREAMON, &enc_type)) {
    perror("Enc Output Start streaming");

    return 1;
  }

  int enc_result_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (ioctl(enc_fd, VIDIOC_STREAMON, &enc_result_type)) {
    perror("Enc Output Start streaming");

    return 1;
  }

  struct v4l2_plane enc_result_planes;
  struct v4l2_buffer enc_result_buf;
  memset(&enc_buf, 0, sizeof(enc_buf));
  enc_result_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  enc_result_buffer.memory = V4L2_MEMORY_MMAP;
  enc_result_buffer.index = 0;
  enc_result_buffer.reserved2 = 0;
  enc_result_buffer.reserved = 0;
  memset(&enc_result_planes, 0, sizeof(enc_result_planes));
  enc_result_buffer.m.planes = &enc_result_planes;
  enc_result_buffer.length = 1;

  if (ioctl(enc_fd, VIDIOC_DQBUF, &enc_result_buffer) == -1) {
    perror("Retrieving encoded frame");
    return 1;
  }

  struct v4l2_buffer displayed_buf;
  displayed_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  displayed_buf.memory = V4L2_MEMORY_DMABUF;
  displayed_buf.reserved2 = 0;
  displayed_buf.reserved = 0;
  displayed_buf.length = 1;
  if (ioctl(enc_fd, VIDIOC_DQBUF, &displayed_buf) == -1) {
    perror("Dequeue displayed encoded frame");
    return 1;
  }


  std::cout << "Got encoded frame!" << std::endl;

  std::ofstream outfile("output.h264", std::ios::binary | std::ios::app);
  outfile.write((char *)enc_buffer, enc_result_buffer.bytesused); // Save encoded frame
  outfile.close();

  if (ioctl(enc_fd, VIDIOC_STREAMOFF, &enc_type)) {
    perror("Stop streaming");

    return 1;
  }

  if (ioctl(enc_fd, VIDIOC_STREAMOFF, &enc_result_type)) {
    perror("Stop streaming");

    return 1;
  }

  if (ioctl(cam_fd, VIDIOC_STREAMOFF, &type)) {
    perror("Stop streaming");

    return 1;
  }
}
