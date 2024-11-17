#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "poll.h"

#include "dmabuf.hpp"

#include <thread>

#include "device_file_handle.hpp"
#include "plog/Log.h"
#include "plog/Initializers/RollingFileInitializer.h"
#include "plog/Appenders/ConsoleAppender.h"

// Camera and Encoder device paths
#define VIDEO_DEVICE "/dev/video0"    // USB camera
#define ENCODER_DEVICE "/dev/video11" // H.264 encoder

struct BufferInfo {
    std::uint32_t index;
    timeval timestamp;
    std::uint32_t bytesused;
    std::uint32_t field;
};

class MappedBuffer {
    void *m_map_ptr;
    std::size_t m_size;

public:
    MappedBuffer(int fd, std::size_t offset, const std::size_t size) : m_size(size) {
        m_map_ptr =
                mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, offset);
        if (m_map_ptr == MAP_FAILED) {
            PLOGE << "Failed to map buffer";
            throw DeviceFileError("Failed to map buffer");
        }
    }

    ~MappedBuffer() {
        munmap(m_map_ptr, m_size);
    }
};


DeviceFileHandle init_camera_device(const std::string &path) {
    return DeviceFileHandle{path};
}

DeviceFileHandle init_encoding_device() {
    return DeviceFileHandle{ENCODER_DEVICE, O_RDWR};
}

v4l2_format set_camera_format(int fd) {
    v4l2_format cam_fmt;
    memset(&cam_fmt, 0, sizeof(cam_fmt));
    cam_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_fmt.fmt.pix.width = 640;
    cam_fmt.fmt.pix.height = 480;
    cam_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cam_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &cam_fmt) == -1) {
        throw DeviceFileError{"Failed to set device format"};
    }

    std::cout << "Actual image width:  " << cam_fmt.fmt.pix.width << std::endl;
    std::cout << "Actual image height: " << cam_fmt.fmt.pix.height << std::endl;
    std::cout << "Actual image format: " << (char *) &cam_fmt.fmt.pix.pixelformat
            << std::endl;
    std::cout << "Actual image size: " << cam_fmt.fmt.pix.sizeimage << std::endl;

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
    // enc_buf.bytesused = info.bytesused;
    enc_buf.field = info.field;
    enc_buf.timestamp = info.timestamp;
    enc_buf.m.planes[0].m.fd =
            dma_buf.get_fd(); // Pass the DMABUF file descriptor
    enc_buf.m.planes[0].length = info.bytesused;

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

    PLOGD << "Dequeued buffer index: " << buf.index;
    PLOGD << "Dequeued buffer bytesused: " << buf.m.planes[0].bytesused;
    PLOGD << "Dequeued buffer field: " << buf.field;

    return buf.index;
}

std::vector<MappedBuffer> query_mplane_mmap_buffers(int fd, std::size_t num_buffers, int buffer_tye) {
    std::vector<MappedBuffer> mmap_buffers;

    for (int i = 0; i < num_buffers; i++) {
        PLOGD << "Querying buffer " << i;
        v4l2_buffer enc_result_buffer = {};
        v4l2_plane enc_planes = {};

        memset(&enc_result_buffer, 0, sizeof(enc_result_buffer));
        enc_result_buffer.type = buffer_tye;
        enc_result_buffer.memory = V4L2_MEMORY_MMAP;
        memset(&enc_planes, 0, sizeof(enc_planes));
        enc_result_buffer.m.planes = &enc_planes;
        enc_result_buffer.length = 1;
        enc_result_buffer.reserved2 = 0;
        enc_result_buffer.reserved = 0;

        if (ioctl(fd, VIDIOC_QUERYBUF, &enc_result_buffer) == -1) {
            PLOGE << "Failed to query buffer";
            throw DeviceFileError{"Failed to query buffer"};
        }

        enc_result_buffer.index = i;

        if (ioctl(fd, VIDIOC_QBUF, &enc_result_buffer) == -1) {
            PLOGE << "Failed to queue buffer";
            throw DeviceFileError{"Failed to queue buffer"};
        }

        PLOGD << "enc_result_buffer.length: "
            << enc_result_buffer.m.planes[0].length;
        PLOGD << "enc_result_buffer.data_offset: "
            << enc_result_buffer.m.planes[0].m.mem_offset;


        mmap_buffers.emplace_back(fd, enc_result_buffer.m.planes[0].m.mem_offset, enc_result_buffer.m.planes[0].length);
    }

    return mmap_buffers;
}

auto cam_capture_to_encoder(const DeviceFileHandle &cam_device, const DeviceFileHandle &enc_device,
                            const std::vector<DmaBuf> &dma_bufs) {
    auto image_buffer_info = cam_device.do_file_operation([](int fd) {
        return dequeue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_DMABUF);
    });

    PLOG_INFO << "Got an Image buffer index: " << image_buffer_info.index;


    enc_device.do_file_operation([&dma_bufs, image_buffer_info](int fd) {
        queue_dma_buffer_mplane(fd, dma_bufs[image_buffer_info.index], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                                image_buffer_info, 0);
    });

    PLOG_INFO << "Queued image dmabuf to encoding device output plane";

    return image_buffer_info;
}

void do_encoding() {
    constexpr std::uint8_t NUM_BUFS{8};

    static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    PLOG_INFO << "Opening camera";
    auto cam_device = init_camera_device("/dev/video0");

    PLOG_INFO << "Camera device opened";

    auto cam_fmt = cam_device.do_file_operation(set_camera_format);

    PLOG_INFO << "Set camera format";

    cam_device.do_file_operation([](int fd) {
        request_buffers(fd, NUM_BUFS, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_DMABUF);
    });

    PLOG_INFO << "Buffers requested";

    auto dma_bufs = allocate_dma_bufs(NUM_BUFS, cam_fmt.fmt.pix.sizeimage);

    PLOG_INFO << "DMA buffers allocated";

    /* enque dmabufs into v4l2 device */
    cam_device.do_file_operation([&dma_bufs](int fd) {
        queue_dma_buffer(fd, dma_bufs, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    });

    PLOG_INFO << "DMA buffers queued";

    cam_device.do_file_operation(stream_on_capture);

    PLOG_INFO << "Stream turned on";


    // Step 6: Open the H.264 encoder
    auto enc_device = init_encoding_device();

    PLOG_INFO << "Encoding device opened";

    // Step 7: Set the encoder format (H.264 output)
    auto enc_fmt_capture = enc_device.do_file_operation(set_encoding_format_capture);
    auto enc_fmt_output = enc_device.do_file_operation(set_encoding_format_output);

    PLOG_INFO << "Encoding device format set";
    PLOGD << "Encoding format sizeimage: " << enc_fmt_capture.fmt.pix.sizeimage;
    PLOGD << "Encoding format sizeimage: " << enc_fmt_output.fmt.pix.sizeimage;

    enc_device.do_file_operation(set_encoding_frame_interval);

    PLOG_INFO << "Encoding device param set";


    // Step 8: Request encoder buffers
    enc_device.do_file_operation([](int fd) {
        request_buffers(fd, 1, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF);
    });


    PLOG_INFO << "Encoding device output Plane buffers requested";

    enc_device.do_file_operation([](int fd) {
        request_buffers(fd, 8, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF);
    });


    PLOG_INFO << "Encoding device capture Plane buffers requested";

    auto enc_dma_bufs = allocate_dma_bufs(8, enc_fmt_capture.fmt.pix.sizeimage);

    enc_device.do_file_operation([&enc_dma_bufs](int fd) {
        queue_dma_buffer_mplane(fd, enc_dma_bufs, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    });


    PLOG_INFO << "Encoding device capture buffer queried";

    enc_device.do_file_operation(stream_on_output_mplane);

    PLOG_INFO << "Encoding output Stream turned on";

    enc_device.do_file_operation(stream_on_capture_mplane);

    PLOG_INFO << "Encoding device turned streams on";

    for (int i = 0; i < 10; i++) {
        auto image_buffer_info = cam_capture_to_encoder(cam_device, enc_device, dma_bufs);

        PLOG_INFO << "run [" << i << "]: " << "Queued image dmabuf to encoding device output plane";

        enc_device.do_file_operation([](int fd) {
            dequeue_buffer_mplane(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF);
        });

        PLOG_INFO << "run [" << i << "]: " << "Dequeued encoding device output buffer";


        auto encoded_capture_buf_index = enc_device.do_file_operation([](int fd) {
            return dequeue_buffer_mplane(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF);
        });

        PLOGD << "Capture buffer index: " << encoded_capture_buf_index;

        PLOG_INFO << "run [" << i << "]: " << "Dequeued encoding device capture buffer";

        enc_device.do_file_operation([&enc_dma_bufs, encoded_capture_buf_index](int fd) {
            queue_dma_buffer_mplane(fd, enc_dma_bufs[encoded_capture_buf_index], V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                                    encoded_capture_buf_index);
        });

        cam_device.do_file_operation([&dma_bufs, image_buffer_info](int fd) {
            queue_dma_buffer(fd, dma_bufs[image_buffer_info.index], V4L2_BUF_TYPE_VIDEO_CAPTURE,
                             image_buffer_info.index);
        });

        PLOG_INFO << "run [" << i << "]: " << "Camera device image dmabuf requeued";

    }

    enc_device.do_file_operation(stream_off_capture_mplane);
    enc_device.do_file_operation(stream_off_output_mplane);
    cam_device.do_file_operation(stream_off_capture);

    PLOG_INFO << "Closed stream";
}

int main() {
    try {
        do_encoding();
    } catch (const std::exception &e) {
        PLOG_ERROR << "Unhandled exception: " << e.what();
    }
}
