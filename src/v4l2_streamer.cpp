//
// Created by nico on 11/17/24.
//

#include "v4l2_streamer.hpp"

#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include "v4l2_operations.hpp"

constexpr auto ENCODER_DEVICE_PATH = "/dev/video11";

V4L2Streamer::V4L2Streamer(const std::string &camera_device_path, std::size_t width,
                           std::size_t height) : m_width(width),
                                                 m_height(height), m_camera(camera_device_path),
                                                 m_encoder(ENCODER_DEVICE_PATH) {
    constexpr std::uint8_t NUM_BUFS{8};

    static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    PLOG_INFO << "Opening camera";

    PLOG_INFO << "Camera device opened";

    auto cam_fmt = m_camera.do_file_operation(set_camera_format);

    PLOG_INFO << "Set camera format";

    m_camera.do_file_operation([](int fd) {
        request_buffers(fd, NUM_BUFS, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_DMABUF);
    });

    PLOG_INFO << "Buffers requested";

    m_camera_capture_buffers = allocate_dma_bufs(NUM_BUFS, cam_fmt.fmt.pix.sizeimage);

    PLOG_INFO << "DMA buffers allocated";

    /* enque dmabufs into v4l2 device */
    m_camera.do_file_operation([this](int fd) {
        queue_dma_buffer(fd, m_camera_capture_buffers, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    });

    PLOG_INFO << "DMA buffers queued";

    auto enc_fmt_capture = m_encoder.do_file_operation(set_encoding_format_capture);
    auto enc_fmt_output = m_encoder.do_file_operation(set_encoding_format_output);

    PLOG_INFO << "Encoding device format set";
    PLOGD << "Encoding format sizeimage: " << enc_fmt_capture.fmt.pix.sizeimage;
    PLOGD << "Encoding format sizeimage: " << enc_fmt_output.fmt.pix.sizeimage;

    m_encoder.do_file_operation(set_encoding_frame_interval);

    PLOG_INFO << "Encoding device param set";


    m_encoder.do_file_operation([](int fd) {
        request_buffers(fd, 1, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF);
    });


    PLOG_INFO << "Encoding device output Plane buffers requested";

    m_encoder.do_file_operation([](int fd) {
        request_buffers(fd, 8, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF);
    });


    PLOG_INFO << "Encoding device capture Plane buffers requested";

    m_encoder_capture_buffers = allocate_dma_bufs(8, enc_fmt_capture.fmt.pix.sizeimage);

    m_encoder.do_file_operation([this](int fd) {
        queue_dma_buffer_mplane(fd, m_encoder_capture_buffers, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    });


    PLOG_INFO << "Encoding device capture buffer queried";
}

void V4L2Streamer::start_streaming() const {
    m_camera.do_file_operation(stream_on_capture);

    PLOGD << "Camera capture stream turned on";

    m_encoder.do_file_operation(stream_on_output_mplane);

    PLOGD << "Encoding output Stream turned on";

    m_encoder.do_file_operation(stream_on_capture_mplane);

    PLOGD << "Encoding capture stream turned on";
}

void V4L2Streamer::next_frame() {
    auto image_buffer_info = m_camera.do_file_operation([](int fd) {
        return dequeue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_DMABUF);
    });

    PLOG_INFO << "Got an Image buffer index: " << image_buffer_info.index;


    m_encoder.do_file_operation([this, image_buffer_info](int fd) {
        queue_dma_buffer_mplane(fd, m_camera_capture_buffers[image_buffer_info.index],
                                V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                                image_buffer_info, 0);
    });

    PLOG_INFO << "Queued image dmabuf to encoding device output plane";

    m_encoder.do_file_operation([](int fd) {
        dequeue_buffer_mplane(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF);
    });

    auto encoded_capture_buf_index = m_encoder.do_file_operation([](int fd) {
        return dequeue_buffer_mplane(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF);
    });

    PLOGD << "Capture buffer index: " << encoded_capture_buf_index;

    m_encoder.do_file_operation([this, encoded_capture_buf_index](int fd) {
        queue_dma_buffer_mplane(fd, m_encoder_capture_buffers[encoded_capture_buf_index],
                                V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                                encoded_capture_buf_index);
    });

    m_camera.do_file_operation([this, image_buffer_info](int fd) {
        queue_dma_buffer(fd, m_camera_capture_buffers[image_buffer_info.index], V4L2_BUF_TYPE_VIDEO_CAPTURE,
                         image_buffer_info.index);
    });
}

V4L2Streamer::~V4L2Streamer() {
    m_encoder.do_file_operation(stream_off_capture_mplane);
    m_encoder.do_file_operation(stream_off_output_mplane);
    m_camera.do_file_operation(stream_off_capture);
    PLOGD << "Streams stopped";
}
