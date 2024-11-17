//
// Copyright (c) 2024 Nico Schmidt
//

#include "v4l2_operations.hpp"

#include <iostream>
#include <plog/Log.h>
#include <sys/ioctl.h>

#include "exceptions.hpp"

v4l2_format set_camera_format(int fd) {
    v4l2_format cam_fmt = {};
    cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_fmt.fmt.pix.width = 640;
    cam_fmt.fmt.pix.height = 480;
    cam_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cam_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &cam_fmt) == -1) {
        throw DeviceFileError{"Failed to set device format"};
    }

    PLOGD << "Actual image width:  " << cam_fmt.fmt.pix.width << std::endl;
    PLOGD << "Actual image height: " << cam_fmt.fmt.pix.height << std::endl;
    PLOGD << "Actual image format: " << (char *) &cam_fmt.fmt.pix.pixelformat
            << std::endl;
    PLOGD << "Actual image size: " << cam_fmt.fmt.pix.sizeimage << std::endl;

    return cam_fmt;
}

v4l2_format set_encoding_format_output(int fd) {
    v4l2_format enc_fmt = {};
    enc_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    enc_fmt.fmt.pix.width = 640;
    enc_fmt.fmt.pix.height = 480;
    enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // Input format: YUYV
    enc_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &enc_fmt) == -1) {
        PLOGE << "Failed to set device output format" << std::strerror(errno);
        throw DeviceFileError{"Failed to set device output format"};
    }

    return enc_fmt;
}

v4l2_format set_encoding_format_capture(int fd) {
    v4l2_format enc_fmt = {};
    enc_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    enc_fmt.fmt.pix.width = 640;
    enc_fmt.fmt.pix.height = 480;
    enc_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264; // Output format: H.264
    enc_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &enc_fmt) == -1) {
        PLOGE << "Failed to set device capture format" << std::strerror(errno);
        throw DeviceFileError{"Failed to set device capture format"};
    }

    PLOGD << "Encoding Capture Actual image width: " << enc_fmt.fmt.pix.width << std::endl;
    PLOGD << "Encoding Capture Actual image height: " << enc_fmt.fmt.pix.height << std::endl;
    PLOGD << "Encoding Capture Actual image sizeimage: " << enc_fmt.fmt.pix.sizeimage << std::endl;

    return enc_fmt;
}

void set_encoding_frame_interval(int fd) {
    v4l2_streamparm stream_parm = {};
    stream_parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    stream_parm.parm.output.timeperframe = {1, 30};

    if (ioctl(fd, VIDIOC_S_PARM, &stream_parm) == -1) {
        PLOGE << "Failed to set device output param" << std::strerror(errno);
        throw DeviceFileError{"Failed to set device output param"};
    }

    PLOGD << "Encoding Output Actual frame interval: " << stream_parm.parm.output.timeperframe.numerator << "/" <<
            stream_parm.parm.output.timeperframe.denominator << std::endl;
}

void request_buffers(int fd, std::uint32_t number_buffers, int buffer_tye, int memory_type) {
    v4l2_requestbuffers cam_req = {};
    cam_req.count = number_buffers;
    cam_req.type = buffer_tye;
    cam_req.memory = memory_type;

    if (ioctl(fd, VIDIOC_REQBUFS, &cam_req) == -1) {
        PLOGE << "Failed to request buffers" << std::strerror(errno);
        throw DeviceFileError{"Failed to request buffers"};
    }

    PLOGD << "Request Buffers allocated count: " << cam_req.count << std::endl;

    if (cam_req.capabilities & V4L2_BUF_CAP_SUPPORTS_DMABUF) {
        PLOGD << "Buffer supports DMABUF";
    } else {
        PLOGW << "Buffer does not support DMABUF";
    }
}

void queue_dma_buffer(int fd, const DmaBuf &dma_buf, std::uint32_t buffer_type, std::uint32_t index) {
    v4l2_buffer buf = {};
    buf.index = index;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.type = buffer_type;
    buf.m.fd = dma_buf.get_fd();

    if (ioctl(fd, VIDIOC_QBUF, &buf)) {
        PLOGE << "Failed to queue dma buffer" << std::strerror(errno);
        throw DeviceFileError{"Failed to queue dma buffer"};
    }
}

void queue_dma_buffer(int fd, const std::vector<DmaBuf> &dma_bufs, std::uint32_t buffer_type) {
    for (int i = 0; i < dma_bufs.size(); i++) {
        PLOGD << "Queue dma buffer " << i;
        queue_dma_buffer(fd, dma_bufs[i], buffer_type, i);
    }
}

void log_buffer_status(int fd, std::uint32_t buffer_type, std::uint32_t index) {
    v4l2_buffer buf = {};
    v4l2_plane planes = {};

    buf.index = index;
    buf.type = buffer_type;
    buf.m.planes = &planes;
    buf.length = 1;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
        PLOGE << "Failed to query buffer: " << std::strerror(errno);
        throw DeviceFileError{"Failed to query buffer"};
    }

    if (buf.flags & V4L2_BUF_FLAG_MAPPED) {
        PLOGD << "Buffer[" << index << "] mapped";
    }
    if (buf.flags & V4L2_BUF_FLAG_PREPARED) {
        PLOGD << "Buffer[" << index << "] prepared";
    }
    if (buf.flags & V4L2_BUF_FLAG_QUEUED) {
        PLOGD << "Buffer[" << index << "] queued";
    }
    if (buf.flags & V4L2_BUF_FLAG_DONE) {
        PLOGD << "Buffer[" << index << "] done";
    }
}

void queue_dma_buffer_mplane(int fd, const DmaBuf &dma_buf, std::uint32_t buffer_tye, std::uint32_t index) {
    PLOGD << "Queue dma buffer mplane " << index;

    log_buffer_status(fd, buffer_tye, index);

    v4l2_plane planes = {};
    v4l2_buffer enc_buf = {};

    enc_buf.index = index;
    enc_buf.type = buffer_tye;
    enc_buf.memory = V4L2_MEMORY_DMABUF;
    enc_buf.m.planes = &planes;
    enc_buf.length = 1;
    enc_buf.m.planes[0].m.fd =
            dma_buf.get_fd(); // Pass the DMABUF file descriptor

    if (ioctl(fd, VIDIOC_QBUF, &enc_buf)) {
        PLOGE << "Failed to queue dma buffer: " << std::strerror(errno);
        throw DeviceFileError{"Failed to queue dma buffer"};
    }
    log_buffer_status(fd, buffer_tye, index);
}

void queue_dma_buffer_mplane(int fd, const DmaBuf &dma_buf, std::uint32_t buffer_tye, const BufferInfo &info,
    std::uint32_t index) {
    PLOGD << "Queue dma buffer mplane " << index;

    log_buffer_status(fd, buffer_tye, index);

    v4l2_plane planes = {};
    v4l2_buffer enc_buf = {};

    enc_buf.index = index;
    enc_buf.type = buffer_tye;
    enc_buf.memory = V4L2_MEMORY_DMABUF;
    enc_buf.m.planes = &planes;
    enc_buf.length = 1;
    enc_buf.bytesused = info.bytesused;
    enc_buf.field = info.field;
    enc_buf.timestamp = info.timestamp;
    enc_buf.m.planes[0].m.fd =
            dma_buf.get_fd(); // Pass the DMABUF file descriptor
    enc_buf.m.planes[0].length = dma_buf.get_size();

    if (ioctl(fd, VIDIOC_QBUF, &enc_buf)) {
        PLOGE << "Failed to queue dma buffer: " << std::strerror(errno);
        throw DeviceFileError{"Failed to queue dma buffer"};
    }

    log_buffer_status(fd, buffer_tye, index);
}

void queue_dma_buffer_mplane(int fd, const std::vector<DmaBuf> &dma_bufs, std::uint32_t buffer_tye) {
    PLOGD << "Queue dma buffers mplane";
    for (int i = 0; i < dma_bufs.size(); i++) {
        queue_dma_buffer_mplane(fd, dma_bufs[i], buffer_tye, i);
    }
}

BufferInfo dequeue_buffer(int fd, std::uint32_t buffer_type, std::uint32_t memory_type) {
    v4l2_buffer buf = {};
    buf.type = buffer_type;
    buf.memory = memory_type;

    if (ioctl(fd, VIDIOC_DQBUF, &buf)) {
        PLOGE << "Failed to dequeue buffer: " << std::strerror(errno);
        throw DeviceFileError{"Failed to dequeue buffer"};
    }

    if (buf.flags & V4L2_BUF_FLAG_LAST) {
        PLOGW << "Last buffer reached";
    }

    if (buf.flags & V4L2_BUF_FLAG_ERROR) {
        PLOGE << "Buffered got an error while dequeueing";
    }

    PLOGD << "Dequeued buffer bytesused: " << buf.bytesused;
    PLOGD << "Dequeued buffer field: " << buf.field;

    return {buf.index, buf.timestamp, buf.bytesused, buf.field};
}

std::uint32_t dequeue_buffer_mplane(int fd, std::uint32_t buffer_type, std::uint32_t memory_type) {
    v4l2_plane planes = {};
    v4l2_buffer buf = {};
    buf.type = buffer_type;
    buf.memory = memory_type;
    buf.m.planes = &planes;
    buf.length = 1;

    PLOGD << "Dequeue buffer mplane";

    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        PLOGE << "Failed to dequeue buffer: " << std::strerror(errno);
        throw DeviceFileError{"Failed to dequeue buffer"};
    }

    log_buffer_status(fd, buffer_type, buf.index);

    if (buf.flags & V4L2_BUF_FLAG_LAST) {
        PLOGW << "Last buffer reached";
    }

    if (buf.flags & V4L2_BUF_FLAG_ERROR) {
        PLOGE << "Buffered got an error while dequeueing";
    }

    PLOGD << "Dequeued file descriptor: " << buf.m.planes[0].m.fd;
    PLOGD << "Dequeued buffer index: " << buf.index;
    PLOGD << "Dequeued buffer bytesused: " << buf.m.planes[0].bytesused;
    PLOGD << "Dequeued buffer field: " << buf.field;

    return buf.index;
}

void stream_on(int fd, std::uint32_t buffer_type) {
    if (ioctl(fd, VIDIOC_STREAMON, &buffer_type)) {
        throw DeviceFileError{"Failed to stream on buffer"};
    }
}

void stream_on_capture(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

void stream_on_output(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
}

void stream_on_output_mplane(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

void stream_on_capture_mplane(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}

void stream_off_capture(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

void stream_off_output(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
}

void stream_off_output_mplane(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

void stream_off_capture_mplane(int fd) {
    stream_on(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}
