#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <cmath>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./generate_gcode_from_image <image_path>\n";
        return 1;
    }

    std::string imagePath = argv[1];
    cv::Mat img = cv::imread(imagePath, cv::IMREAD_GRAYSCALE);
    if (img.empty()) {
        std::cerr << "Failed to load image\n";
        return 1;
    }

    cv::resize(img, img, cv::Size(180, 180));

    cv::Mat blurred;
    cv::GaussianBlur(img, blurred, cv::Size(5, 5), 0);


    cv::Mat bw;
    cv::threshold(blurred, bw, 128, 255, cv::THRESH_BINARY_INV);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::dilate(bw, bw, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bw, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::ofstream gcodeFile("output.gcode");
    if (!gcodeFile) {
        std::cerr << "Error opening output file\n";
        return 1;
    }

    gcodeFile << "G21 ; Set units to mm\n";
    gcodeFile << "G90 ; Absolute positioning\n";
    gcodeFile << "G0 Z5 ; Pen up\n";

    double scale = 1;

    for (const auto& contour : contours) {
        if (contour.empty() || cv::contourArea(contour) < 50) continue;

        std::vector<cv::Point> approx;
        double epsilon = 1.5;
        cv::approxPolyDP(contour, approx, epsilon, true);

        if (approx.empty()) continue;

        int startX = static_cast<int>(std::round(approx[0].x * scale));
        int startY = static_cast<int>(std::round(approx[0].y * scale));

        gcodeFile << "G0 Z5 ; Pen up\n";
        gcodeFile << "G0 X" << startX << " Y" << startY << "\n";
        gcodeFile << "G1 Z0 ; Pen down\n";

        for (size_t i = 1; i < approx.size(); ++i) {
            int X = static_cast<int>(std::round(approx[i].x * scale));
            int Y = static_cast<int>(std::round(approx[i].y * scale));
            gcodeFile << "G1 X" << X << " Y" << Y << "\n";
        }

        gcodeFile << "G1 X" << startX << " Y" << startY << "\n";
        gcodeFile << "G0 Z5 ; Pen up\n";
    }

    gcodeFile << "M30 ; Program end\n";
    gcodeFile.close();

    std::cout << "G-code written to 'output.gcode' with resized 180x180 input\n";
    return 0;
}
