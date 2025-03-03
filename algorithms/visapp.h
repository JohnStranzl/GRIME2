/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Copyright 2021 Kenneth W. Chapman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/** \file visapp.h
 * @brief Business logic of the gaugecam waterlevel find system
 *
 * This file holds a class that performs higher level functions that make the
 * the GaugeCam libraries easier to use.
 *
 * \author Kenneth W. Chapman
 * \copyright Copyright (C) 2010-2021, Kenneth W. Chapman <coffeesig@gmail.com>, all rights reserved.\n
 * This project is released under the Apache License, Version 2.0.
 * \bug No known bugs.
 */

#ifndef VISAPP_H
#define VISAPP_H

#include "calibexecutive.h"
#include "findline.h"
#include "metadata.h"
#include "animate.h"

//! GaugeCam classes, functions and variables
namespace gc
{

static const std::string GAUGECAM_VISAPP_VERSION = "0.0.0.4";           ///< GaugeCam executive logic (VisApp) software version

/**
 * @brief Business logic that instantiates objects of the GaugeCam classes and provides methods
 * to make their use more straightforward
 */
class VisApp
{
public:
    /**
     * @brief Constructor, initializes the bowtie templates for target searches and creates scratch folders
     */
    VisApp();

    /**
     * @brief Destructor
     */
    ~VisApp() {}

    /**
     * @brief Retreive the current software version of VisApp (executive logic class)
     * @return String holding the version
     */
    static std::string Version() { return GAUGECAM_VISAPP_VERSION; }

    /**
     * @brief Print the ExifTool (command line program) to stdout
     */
    static void GetExifToolVersion() { MetaData::GetExifToolVersion(); }


    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Calibration methods
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    /**
     * @brief Set the current calibration from a calibration model json file
     * @param imgFilepath The filepath of the image with the calibration target
     * @param jsonControl Json string that controls the calibration
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS Calibrate( const string imgFilepath, const string jsonControl,
                         double &rmseDist, double &rmseX, double &rmseY );

    // TODO: Add doxygen comments
    GC_STATUS Calibrate( const string imgFilepath, const string jsonControl, const string resultImgPath,
                         double &rmseDist, double &rmseX, double &rmseY );

    // TODO: Adjust doxygen comments
    /**
     * @brief Set the current calibration from a calibration model json file
     * @param calibJson The filepath of the calibration model json file
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS LoadCalib( const std::string calibJson );

    /**
     * @brief Convert world coordinates to pixel coordinates using the currently set calibration
     * @param worldPt World coordinate xy position
     * @param pixelPt Point to hold the converted pixel coordinate position
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS WorldToPixel( const cv::Point2d worldPt, cv::Point2d &pixelPt );

    // TODO: Add doxygen comments here
    GC_STATUS FindBowtieResiduals( const std::string imgFilepath, double &rmseX, double &rmseY, double &rmseEucDist,
                                   std::vector< cv::Point2d > &pixPts, std::vector< cv::Point2d > &worldPts,
                                   std::vector< cv::Point2d > &pixPtsReverse );
    /**
     * @brief Convert pixel coordinates to world coordinates using the currently set calibration
     * @param pixelPt Pixel coordinate xy position
     * @param worldPt Point to hold the converted world coordinate position
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS PixelToWorld( const cv::Point2d pixelPt, cv::Point2d &worldPt );

    /**
     * @brief Draw the currently loaded calibration onto an overlay image
     * @param imgMatOut OpenCV Mat of the input image onto which the calibration will be written
     * @param imageMatOut OpenCV Mat of the output image that holds the input image with an overlay of the calibration
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */

    // TODO: Fix doxygen
    GC_STATUS SetStopsignColor( const cv::Scalar color, const double minRange, const double maxRange, cv::Scalar &hsv );
    GC_STATUS DrawCalibOverlay( const cv::Mat matIn, cv::Mat &imgMatOut );
    GC_STATUS DrawCalibOverlay( const cv::Mat matIn, cv::Mat &imgMatOut,
                                const bool drawCalib, const bool drawMoveROIs, const bool drawSearchROI );

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Findline methods
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    /**
     * @brief Find the water level in an image specified in the FindLineParams (results to member result
     *        object and to provided result object)
     * @param params Holds the filepaths and all other parameters need to perform a line find calculation
     * @param timestamp Image capture time
     * @param result Holds the results of the line find calculation
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS CalcLine( const FindLineParams params, FindLineResult &result );

    /**
     * @brief Find the water level in an image specified in the FindLineParams (results to member result object)
     * @param params Holds the filepaths and all other parameters need to perform a line find calculation
     *        class member object holds results of the line find calculation
     * @param timestamp Image capture time
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS CalcLine( const FindLineParams params );

    /**
     * @brief Find the water level in the specified image
     * @param img OpenCV mat image to search for the waterline using already set parameters
     * @param timestamp Image capture time
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS CalcLine( const cv::Mat &img, const string timestamp );

    /**
     * @brief Find the water level in the specified image
     * @param img OpenCV mat image to search for the waterline using already set parameters
     * @param resultJson Result of the water level find in json format
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS CalcLine( const FindLineParams params, FindLineResult &result, string &resultJson );

    /**
     * @brief Get image exif data used by GaugeCam as a human readable string
     * @param filepath Filepath of the image from which to retrieve the exif dat
     * @param data Human readable data from the image exif data
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS GetImageData( const std::string filepath, string &data );

    /**
     * @brief Get image exif data used by GaugeCam into a data object
     * @param filepath Filepath of the image from which to retrieve the exif data
     * @param exifFeat Instance of class to hold the exif data
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS GetImageData( const std::string filepath, ExifFeatures &exifFeat );

    /**
     * @brief Get the image capture timestamp string from the image exif data
     * @param filepath Filepath of the image from which to retrieve the timestamp
     * @param timestamp String to hold the timestamp
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS GetImageTimestamp( const std::string filepath, std::string &timestamp );

    /**
     * @brief Sets the GIF output filepath and other initializations
     * @param imgSize Expected image size for all frames
     * @param imgCount Count of expected images in the GIF to assure there are enough resources to write the GIF
     * @param gifFilepath Sets the GIF output filepath
     * @param delay_ms Delay in milliseconds between image frames
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     * @see AddImageToGIF(), EndGIF()
     */
    GC_STATUS BeginGIF( const cv::Size imgSize, const int imgCount, const std::string gifFilepath, const int delay_ms );

    /**
     * @brief Adds an image to the GIF initalized by a call to BeginGIF()
     * @param img The image to be added to the GIF
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     * @see BeginGIF(), EndGIF()
     */
    GC_STATUS AddImageToGIF( const cv::Mat &img );

    /**
     * @brief Closes the GIF file initialized by BeginGIF() and frees resources
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     * @see BeginGIF(), AddImageToGIF()
     */
    GC_STATUS EndGIF();

    /**
     * @brief Create an overlay image with the current found water line
     * @param img Source OpenCV Mat image that was searched for a water line
     * @param imgOut Destination OpenCV Mat image holding the source image with find line overlay
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS DrawLineFindOverlay( const cv::Mat &img, cv::Mat &imgOut, const LineDrawType overlayTypes = NO_OVERLAY );

    /**
     * @brief Create an overlay image with a user specified found water line
     * @param img Source OpenCV Mat image that was searched for a water line
     * @param imgOut Destination OpenCV Mat image holding the source image with find line overlay
     * @param findLineResult User specified found line result object
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS DrawLineFindOverlay( const cv::Mat &img, cv::Mat &imgOut, const FindLineResult findLineResult,
                                   const LineDrawType overlayTypes = static_cast< LineDrawType >( static_cast< int >( FOUND_LINE ) |
                                                                                                  static_cast< int >( RANSAC_POINTS ) ) );

    /**
     * @brief Get the current found line position
     * @return Internal FindLineResult object
     */
    FindLineResult GetFindLineResult() { return m_findLineResult; }

    /**
     * @brief Create and/or append find line results fo a csv file
     *
     * A header is added to the csv file when it is created.
     *
     * @param resultCSV Output filepath of the csv file to be created or appended
     * @param imgPath Filepath of the image to which the results apply
     * @param result The results of the waterlevel calculation to be appended
     * @param overwrite true=overwrite the csv file destroying what was previously there
     * false=append the data to the file if exists and create a new one if it does not
     * @return GC_OK=Success, GC_FAIL=Failure, GC_EXCEPT=Exception thrown
     */
    GC_STATUS WriteFindlineResultToCSV( const std::string resultCSV, const std::string imgPath,
                                        const FindLineResult &result, const bool overwrite = false );

private:
    std::string m_calibFilepath;

    CalibExecutive m_calibExec;
    FindLine m_findLine;
    FindLineResult m_findLineResult;
    MetaData m_metaData;
    Animate m_animate;

    GC_STATUS CalcFindLine( const cv::Mat &img, FindLineResult &result );
    GC_STATUS AdjustSearchAreaForMovement( const std::vector< LineEnds > &searchLines,
                                           std::vector< LineEnds > &searchLinesAdj, const cv::Point2d offsets );
    GC_STATUS PixelToWorld( FindPointSet &ptSet );
    GC_STATUS FindPtSet2JsonString( const FindPointSet set, const string set_type, string &json );
};

} // namespace gc

#endif // VISAPP_H
