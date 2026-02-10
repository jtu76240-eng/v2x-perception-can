/*
 * Copyright Telechips Inc.
 *
 * TCC Version 1.0
 *
 * This source code contains confidential information of Telechips.
 *
 * Any unauthorized use without a written permission of Telechips including not
 * limited to re-distribution in source or binary form is strictly prohibited.
 *
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind, including without
 * limitation, any warranty of merchantability, fitness for a particular purpose
 * or non-infringement of any patent, copyright or other third party intellectual
 * property right.
 * No warranty is made, express or implied, regarding the information's accuracy,
 * completeness, or performance.
 *
 * In no event shall Telechips be liable for any claim, damages or other
 * liability arising from, out of or in connection with this source code or
 * the use in the source code.
 *
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 */

#include <iostream>
#include <sys/time.h>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "opencv_api.h"

static cv::Scalar ccolorArray[NETWORK_INDEX_MAX] = {cv::Scalar(0, 255, 0), cv::Scalar(0, 0, 255)};

/**
 * @brief Draws dynamic information (FPS, Network Info, Memory Usage, NPU Info) on an image.
 *
 * This function draws various types of information (e.g., FPS, Network Info, Memory Usage, NPU Info)
 * on an image at a specified location with adjustable font size, color, and position based on the scaling
 * of the destination size. The text is drawn on the image using the specified parameters, including type of
 * information, font size, color, and position.
 *
 * @param desBuffer A pointer to the destination image buffer (in RGB format).
 * @param width The width of the destination image (in pixels).
 * @param height The height of the destination image (in pixels).
 * @param infoType The type of information to be displayed (e.g., "FPS", "Network", "Memory", or "NPU").
 * @param value The value to be displayed (e.g., FPS value, network time in ms, memory usage in %, NPU utilization in %).
 * @param deviceIdx The index of the device for Network and NPU information (e.g., network index, NPU index).
 * @param xPos The x-axis position (in percentage of image width) where the text will be drawn.
 * @param yPos The y-axis position (in percentage of image height) where the text will be drawn.
 * @param textColor The color of the text in BGR format (e.g., `cv::Scalar(255, 0, 0)` for blue).
 * @param fontSize A scaling factor for the font size, based on the image dimensions.
 *
 * @return None. This function does not return a value.
 *
 * @note The x and y positions are provided as percentages of the image width and height, and are scaled
 * accordingly based on the destination image dimensions.
 *
 * @note The function handles different types of information based on the @p infoType string. Valid values for
 * @p infoType are "FPS", "Network", "Memory", and "NPU".
 */
void cvDrawInfo(unsigned char *desBuffer, uint32_t outputWidth, uint32_t outputHeight,
				draw_info_t infoType, double value, int deviceIdx,
				int xPos, int yPos, double fontSize, const Color_t textColor)
{
	float wScale = static_cast<float>(outputWidth) / BASE_WIDTH;
	float hScale = static_cast<float>(outputHeight) / BASE_HEIGHT;

	cv::Mat image(cv::Size(outputWidth, outputHeight), CV_8UC3, desBuffer);

	char buf[30];

	if (infoType == DRAW_INFO_FPS)
	{
		sprintf(buf, "FPS: %.1f", value);
	}
	else if (infoType == DRAW_INFO_NETWORK)
	{
		sprintf(buf, "Network %d: %dms", deviceIdx, static_cast<int>(value));
	}
	else if (infoType == DRAW_INFO_CPU)
	{
		sprintf(buf, "CPU Usg: %d%%", static_cast<int>(value));
	}
	else if (infoType == DRAW_INFO_MEMORY)
	{
		sprintf(buf, "Mem Usg: %d%%", static_cast<int>(value));
	}
	else if (infoType == DRAW_INFO_NPU)
	{
		sprintf(buf, "NPU %d Util: %d%%", deviceIdx, static_cast<int>(value));
	}
	else
	{
		return; // Return if infoType is not recognized
	}

	int scaledXPos = xPos * wScale;
	int scaledYPos = yPos * hScale;

	// Draw the text on the image
	cv::putText(image, buf, cv::Point(scaledXPos, scaledYPos), cv::FONT_HERSHEY_SIMPLEX, fontSize * wScale, cv::Scalar(textColor.b, textColor.g, textColor.r));
	// cv::putText(image, buf, cv::Point(scaledXPos, scaledYPos), cv::FONT_HERSHEY_SIMPLEX, 1.2 * wScale, cv::Scalar(textColor.b, textColor.g, textColor.r));
}

/**
 * @brief Draws bounding boxes and class labels on an image with dynamic parameters.
 *
 * This function draws bounding boxes and class labels on an image at specified locations
 * with adjustable font size, color, and position based on the scaling of the destination size.
 *
 * @param desBuffer A pointer to the destination image buffer (in RGB format).
 * @param boxes An array of `box_t` structs containing the bounding boxes and class information.
 * @param boxCount The number of bounding boxes to be drawn.
 * @param width The width of the destination image (in pixels).
 * @param height The height of the destination image (in pixels).
 * @param boxColor The color of the bounding box (e.g., `cv::Scalar(255, 0, 0)` for blue).
 * @param textColor The color of the text in BGR format (e.g., `cv::Scalar(255, 0, 0)` for blue).
 * @param fontSize A scaling factor for the font size, based on the image dimensions.
 * @param textOffset The y-axis offset for the label text from the bounding box (relative to `yMin`).
 *
 * @return None. This function does not return a value.
 */
void cvDrawBoxes(unsigned char *desBuffer, Box_t *boxes, int boxCount,
				 uint32_t outputWidth, uint32_t outputHeight, uint32_t boxImageWidth, uint32_t boxImageHeight,
				 const Color_t boxColor, const Color_t textColor,
				 double fontSize, int textOffset)
{
	char buf[30];
	cv::Mat image(cv::Size(outputWidth, outputHeight), CV_8UC3, desBuffer);

	for (int boxIdx = 0; boxIdx < boxCount; boxIdx++)
	{
		/* Coordinates of the box */
		int xMin = boxes[boxIdx].xmin * ((float)outputWidth / boxImageWidth);
		int yMin = boxes[boxIdx].ymin * ((float)outputHeight / boxImageHeight);
		int xMax = boxes[boxIdx].xmax * ((float)outputWidth / boxImageWidth);
		int yMax = boxes[boxIdx].ymax * ((float)outputHeight / boxImageHeight);

		xMin = std::max(0, std::min(xMin, static_cast<int>(outputWidth)));
		yMin = std::max(0, std::min(yMin, static_cast<int>(outputHeight)));
		xMax = std::max(0, std::min(xMax, static_cast<int>(outputWidth)));
		yMax = std::max(0, std::min(yMax, static_cast<int>(outputHeight)));

		/* Prepare the label text with class ID and score */
		sprintf(buf, "[OBJ][%d][%.3lf]", boxes[boxIdx].cls, boxes[boxIdx].score);
		/* Draw the bounding box on the image */
		cv::rectangle(image, cv::Rect(cv::Point(xMin, yMin), cv::Point(xMax, yMax)), cv::Scalar(boxColor.b, boxColor.g, boxColor.r), 1, 8, 0);

		/* Draw the label text near the bounding box */
		cv::putText(image, buf, cv::Point(xMin, yMin - textOffset), cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(textColor.b, textColor.g, textColor.r));
		// cv::putText(image, buf, cv::Point(xMin, yMin - textOffset), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(textColor.b, textColor.g, textColor.r));
	}
}

/**
 * @brief Draws a class label on an image with dynamic parameters.
 *
 * This function draws a class label on the image at a specified location with
 * adjustable font size, color, and position based on the scaling of the destination size.
 *
 * @param desBuffer A pointer to the destination image buffer (in RGB format).
 * @param width The width of the destination image (in pixels).
 * @param height The height of the destination image (in pixels).
 * @param className The name of the class.
 * @param xPos The x-axis position where the class label will be drawn.
 * @param yPos The y-axis position where the class label will be drawn.
 * @param textColor The color of the text in BGR format (e.g., `cv::Scalar(255, 0, 0)` for blue).
 * @param fontSize A scaling factor for the font size, based on the image dimensions.
 *
 * @return None. This function does not return a value.
 */
void cvDrawCls(unsigned char *desBuffer, uint32_t width, uint32_t height,
			   const int className, int xPos, int yPos,
			   const Color_t textColor, double fontSize)
{
	float wScale = static_cast<float>(width) / BASE_WIDTH;
	float hScale = static_cast<float>(height) / BASE_HEIGHT;

	cv::Mat image(cv::Size(width, height), CV_8UC3, desBuffer);

	char buf[30];
	sprintf(buf, "[CLS][%d]", className);

	int scaledXPos = xPos * wScale;
	int scaledYPos = yPos * hScale;

	cv::putText(image, buf, cv::Point(scaledXPos, scaledYPos), cv::FONT_HERSHEY_SIMPLEX, fontSize * wScale, cv::Scalar(textColor.b, textColor.g, textColor.r));
}

/**
 * @brief Loads an image file and converts it to RGBA format, storing the result in a buffer.
 *
 * This function reads an image file from the specified path, converts the image data from BGR to RGB,
 * and then converts it to RGBA format, storing the result in the provided `desBuffer`. The function can also
 * return the image's width and height through pointer arguments.
 *
 * @param desBuffer A buffer to store the converted RGBA image data.
 *                  This buffer should be allocated with enough space to hold the image data.
 * @param filePath The path of the image file to be loaded.
 * @param width A pointer to a variable where the width of the image will be stored.
 *              If `nullptr`, the width is not returned.
 * @param height A pointer to a variable where the height of the image will be stored.
 *               If `nullptr`, the height is not returned.
 *
 * @return The total number of bytes of the image. Returns `-1` in case of failure.
 *
 * @note This function uses `cv::imread` to load the image and `cv::cvtColor` to convert the color space.
 *       The returned byte count is `width * height * 4` (for RGBA format with 4 channels).
 */
int cvLoadImage(unsigned char *desBuffer, const char *filePath, uint32_t *width, uint32_t *height)
{
	cv::Mat image = cv::imread(filePath, cv::IMREAD_COLOR);
	if (image.empty())
	{
		std::cerr << "Invalid image file: " << filePath << std::endl;
		return -1;
	}
	cv::Mat imageRGBA;
	cv::cvtColor(image, imageRGBA, cv::COLOR_RGB2RGBA);

	if (width != nullptr && height != nullptr)
	{
		*width = static_cast<uint32_t>(imageRGBA.cols);
		*height = static_cast<uint32_t>(imageRGBA.rows);
	}

	memcpy(desBuffer, imageRGBA.data, imageRGBA.total() * imageRGBA.elemSize());
	return static_cast<int>(imageRGBA.total() * imageRGBA.elemSize());
}

/**
 * @brief Saves an image from the buffer to a file in the specified format.
 *
 * This function takes an image stored in a buffer, converts it into a `cv::Mat` object, and saves it to the
 * specified file path in a standard image format (e.g., PNG, JPG). The function avoids potential memory access
 * issues by creating a deep copy of the image before saving.
 *
 * @param srcBuffer A pointer to the buffer containing the image data. This buffer must contain the image data
 *                  in RGB format with dimensions specified by `width` and `height`.
 * @param filePath The path to the file where the image will be saved. The function will attempt to write the image
 *                 to this file location.
 * @param width The width of the image (in pixels).
 * @param height The height of the image (in pixels).
 *
 * @return None. This function does not return a value.
 *
 * @note This function uses `cv::imwrite` to save the image and performs a deep copy of the `cv::Mat` object
 *       to avoid potential issues with memory access (bus errors).
 *       The image is assumed to be in RGB format (with 3 channels).
 */
void cvSaveImage(unsigned char *srcBuffer, const char *filePath, const uint32_t width, const uint32_t height)
{
	cv::Mat image(cv::Size(width, height), CV_8UC3, srcBuffer);
	if (!cv::imwrite(filePath, image))
	{
		std::cerr << "Failed to save image to " << filePath << std::endl;
	}
}

/* OpenCV 기본은 BGR. 효율적으로 쓰려면 BGR로 저장 → imread() 시 변환 없음, drawing도 BGR 기준 */
void cvSaveImageEx(unsigned char *srcBuffer, const char *filePath, uint32_t width, uint32_t height, int channels)
{
	if (channels == 3)
	{
		cv::Mat image(cv::Size(width, height), CV_8UC3, srcBuffer);
		if (!cv::imwrite(filePath, image))
			std::cerr << "Failed to save image to " << filePath << std::endl;
	}
	else if (channels == 4)
	{
		cv::Mat rgba(cv::Size(width, height), CV_8UC4, srcBuffer);
		cv::Mat bgr;
		cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
		if (!cv::imwrite(filePath, bgr))
			std::cerr << "Failed to save image to " << filePath << std::endl;
	}
}

/* 맵 생성용 내부 함수 (map_x, map_y 채우기). K[9], D[5]=표준 계수 */
static void fisheyeBuildMaps(double fx, double fy, double cx, double cy,
	const double *D, int width, int height, cv::Mat *map_x, cv::Mat *map_y)
{
	double k1 = D[0], k2 = D[1], p1 = D[2], p2 = D[3], k3 = (D[4] != 0.0 ? D[4] : 0.0);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			double u = (x - cx) / fx;
			double v = (y - cy) / fy;
			double r2 = u * u + v * v;
			double r4 = r2 * r2;
			double r6 = r2 * r4;
			double radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
			double u_dist = u * radial + 2.0 * p1 * u * v + p2 * (r2 + 2.0 * u * u);
			double v_dist = v * radial + p1 * (r2 + 2.0 * v * v) + 2.0 * p2 * u * v;
			map_x->at<float>(y, x) = (float)(u_dist * fx + cx);
			map_y->at<float>(y, x) = (float)(v_dist * fy + cy);
		}
	}
}

/* 기본 OpenCV만 사용 (imgproc의 remap). calib3d 불필요. K[9], D[5]=표준 계수 (k1,k2,p1,p2,k3) */
void cvFisheyeUndistort(unsigned char *src, unsigned char *dst, uint32_t width, uint32_t height, int channels, const float *K, const float *D)
{
	if (src == NULL || dst == NULL || K == NULL || D == NULL || (channels != 3 && channels != 4))
		return;

	double fx = (double)K[0], fy = (double)K[4];
	double cx = (double)K[2], cy = (double)K[5];
	double Dd[5] = { (double)D[0], (double)D[1], (double)D[2], (double)D[3], (double)(D[4] != 0.f ? D[4] : 0.f) };

	cv::Mat map_x(height, width, CV_32FC1);
	cv::Mat map_y(height, width, CV_32FC1);
	fisheyeBuildMaps(fx, fy, cx, cy, Dd, (int)width, (int)height, &map_x, &map_y);

	int cv_type = (channels == 3) ? CV_8UC3 : CV_8UC4;
	cv::Mat srcMat(cv::Size(width, height), cv_type, src);
	cv::Mat dstMat(cv::Size(width, height), cv_type, dst);
	cv::remap(srcMat, dstMat, map_x, map_y, cv::INTER_LINEAR);
}

struct FisheyeMapHolder {
	cv::Mat map_x;
	cv::Mat map_y;
};

void *cvFisheyeUndistortMapCreate(const float *K, const float *D, uint32_t width, uint32_t height)
{
	if (K == NULL || D == NULL || width == 0 || height == 0)
		return NULL;

	double fx = (double)K[0], fy = (double)K[4];
	double cx = (double)K[2], cy = (double)K[5];
	double Dd[5] = { (double)D[0], (double)D[1], (double)D[2], (double)D[3], (double)(D[4] != 0.f ? D[4] : 0.f) };

	FisheyeMapHolder *h = new FisheyeMapHolder();
	h->map_x = cv::Mat(height, width, CV_32FC1);
	h->map_y = cv::Mat(height, width, CV_32FC1);
	fisheyeBuildMaps(fx, fy, cx, cy, Dd, (int)width, (int)height, &h->map_x, &h->map_y);
	return (void *)h;
}

void cvFisheyeUndistortRemap(unsigned char *src, unsigned char *dst, uint32_t width, uint32_t height, int channels, void *mapHandle)
{
	if (src == NULL || dst == NULL || mapHandle == NULL || (channels != 3 && channels != 4))
		return;

	FisheyeMapHolder *h = (FisheyeMapHolder *)mapHandle;
	int cv_type = (channels == 3) ? CV_8UC3 : CV_8UC4;
	cv::Mat srcMat(cv::Size(width, height), cv_type, src);
	cv::Mat dstMat(cv::Size(width, height), cv_type, dst);
	cv::remap(srcMat, dstMat, h->map_x, h->map_y, cv::INTER_LINEAR);
}

void cvFisheyeUndistortMapRelease(void **mapHandle)
{
	if (mapHandle == NULL || *mapHandle == NULL)
		return;
	delete (FisheyeMapHolder *)*mapHandle;
	*mapHandle = NULL;
}

/**
 * @brief Resizes an image from the source buffer to the destination buffer in the specified format.
 *
 * This function reads an image from the source buffer, resizes it to the specified dimensions,
 * and stores the resized image in the destination buffer. The resizing is performed using OpenCV's
 * `cv::resize` function. The image format can be RGB (3 channels), RGBA (4 channels), or Grayscale (1 channel).
 *
 * @param srcBuffer A pointer to the source image data. This buffer must contain the image in the specified format
 *                  with dimensions specified by `srcW` and `srcH`. The source image can be in RGB, RGBA, or Grayscale format.
 * @param srcW The width of the source image (in pixels).
 * @param srcH The height of the source image (in pixels).
 * @param desBuffer A pointer to the destination buffer where the resized image data will be stored.
 *                  This buffer must be allocated with enough space to hold the resized image data.
 * @param desW The width of the destination image (in pixels).
 * @param desH The height of the destination image (in pixels).
 * @param channels The number of channels in the source image. Should be 1 (Grayscale), 3 (RGB), or 4 (RGBA).
 *
 * @return None. This function does not return a value.
 *
 * @note The destination buffer must have enough space allocated to store the resized image.
 */
void cvResize(unsigned char *srcBuffer, uint32_t srcW, uint32_t srcH,
			  unsigned char *desBuffer, uint32_t desW, uint32_t desH, int channels)
{
	cv::Mat originImage(cv::Size(srcW, srcH), CV_8UC(channels), srcBuffer);
	cv::Mat resizedImage(desH, desW, CV_8UC(channels));
	cv::resize(originImage, resizedImage, cv::Size(desW, desH));
	std::memcpy(desBuffer, resizedImage.data, desW * desH * channels);
}

void cvDrawLines(unsigned char *desBuffer, uint32_t width, uint32_t height,
				 Point_t *P1, Point_t *P2, int N, const Color_t color, int thickness)
{
	cv::Mat image(cv::Size(width, height), CV_8UC3, desBuffer);

	for (int i = 0; i < N; i++)
	{
		cv::Point start(P1[i].x, P1[i].y);
		cv::Point end(P2[i].x, P2[i].y);

		cv::line(image, start, end, cv::Scalar(color.b, color.g, color.r), thickness);
	}
}

/**
 * @brief Draws a series of points (coordinates) on an image.
 *
 * This function draws circles at the given set of points on an image. The points are drawn at
 * the specified positions, and the size and color of the circles are adjustable.
 *
 * @param desBuffer A pointer to the destination image buffer (in RGB format).
 * @param width The width of the destination image (in pixels).
 * @param height The height of the destination image (in pixels).
 * @param points An array of custom `point` structures representing the points to be drawn on the image.
 * @param numPoints The number of points in the `points` array.
 * @param pointRadius The radius of the circles drawn at each point.
 * @param pointColor The color of the points (circles) in BGR format (e.g., `cv::Scalar(0, 0, 255)` for red).
 *
 * @return None. This function does not return a value.
 *
 * @note The points are drawn at the specified pixel positions, and their positions are scaled
 * based on the image size.
 */
void cvDrawPoints(unsigned char *desBuffer, uint32_t width, uint32_t height,
				  Point_t *points, int numPoints, int pointRadius, const Color_t pointColor)
{
	cv::Mat image(cv::Size(width, height), CV_8UC3, desBuffer);

	for (int i = 0; i < numPoints; i++)
	{
		cv::circle(image, cv::Point(points[i].x, points[i].y), pointRadius, cv::Scalar(pointColor.b, pointColor.g, pointColor.r), -1);
	}
}

/**
 * @brief Draws custom text on an image at a specified location.
 *
 * This function allows the user to draw a custom string on an image at a specified location with
 * adjustable font size, color, and position based on the scaling of the destination size.
 * The text is drawn using the specified parameters, including font size, color, and position.
 *
 * @param desBuffer A pointer to the destination image buffer (in RGB format).
 * @param width The width of the destination image (in pixels).
 * @param height The height of the destination image (in pixels).
 * @param customText The custom string to be displayed on the image.
 * @param xPos The x-axis position (in percentage of image width) where the text will be drawn.
 * @param yPos The y-axis position (in percentage of image height) where the text will be drawn.
 * @param textColor The color of the text in BGR format (e.g., `cv::Scalar(255, 0, 0)` for blue).
 * @param fontSize A scaling factor for the font size, based on the image dimensions.
 *
 * @return None. This function does not return a value.
 *
 * @note The x and y positions are provided as percentages of the image width and height, and are scaled
 * accordingly based on the destination image dimensions.
 */
void cvDrawCustomText(unsigned char *desBuffer, uint32_t width, uint32_t height,
					  const char *customText, int xPos, int yPos,
					  const Color_t textColor, double fontSize)
{
	float wScale = static_cast<float>(width) / BASE_WIDTH;
	float hScale = static_cast<float>(height) / BASE_HEIGHT;

	cv::Mat image(cv::Size(width, height), CV_8UC3, desBuffer);

	int scaledXPos = xPos * wScale;
	int scaledYPos = yPos * hScale;

	cv::putText(image, customText, cv::Point(scaledXPos, scaledYPos), cv::FONT_HERSHEY_SIMPLEX, fontSize * wScale, cv::Scalar(textColor.b, textColor.g, textColor.r));
}

/* ---------- RGB → BGR 복사 (SHM/소켓 등 OpenCV 소비자용으로 경계에서 1회 변환) ---------- */
void cvRgbToBgrCopy(const unsigned char *src, unsigned char *dst, uint32_t width, uint32_t height)
{
	if (src == NULL || dst == NULL || width == 0 || height == 0)
		return;
	cv::Mat rgb(height, width, CV_8UC3, const_cast<unsigned char *>(src));
	cv::Mat bgr;
	cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
	std::memcpy(dst, bgr.data, (size_t)width * height * 3);
}

void cvRotate180(unsigned char *buffer, uint32_t width, uint32_t height, int channels)
{
	if (buffer == NULL || width == 0 || height == 0)
		return;
	cv::Mat img(height, width, CV_8UC(channels), buffer);
	cv::rotate(img, img, cv::ROTATE_180);
}

/* ---------- VideoWriter (비동기 영상 녹화용). buffer는 RGB, AVI에도 RGB로 기록 ---------- */
void *cvVideoWriterCreate(const char *filePath, uint32_t width, uint32_t height, double fps)
{
	if (filePath == NULL || width == 0 || height == 0 || fps <= 0.0)
		return NULL;
	cv::VideoWriter *vw = new cv::VideoWriter();
	int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
	if (!vw->open(filePath, fourcc, fps, cv::Size(width, height)))
	{
		delete vw;
		return NULL;
	}
	return static_cast<void *>(vw);
}

int cvVideoWriterWrite(void *writer, unsigned char *buffer, uint32_t width, uint32_t height)
{
	if (writer == NULL || buffer == NULL)
		return -1;
	cv::VideoWriter *vw = static_cast<cv::VideoWriter *>(writer);
	cv::Mat rgb(cv::Size(width, height), CV_8UC3, buffer);
	vw->write(rgb);  /* AVI에는 RGB로 저장 */
	return 0;
}

void cvVideoWriterRelease(void **writer)
{
	if (writer == NULL || *writer == NULL)
		return;
	cv::VideoWriter *vw = static_cast<cv::VideoWriter *>(*writer);
	vw->release();
	delete vw;
	*writer = NULL;
}