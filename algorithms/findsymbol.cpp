#include "log.h"
#include "findsymbol.h"
#include <opencv2/imgproc.hpp>
#include <limits>

#ifndef DEBUG_FIND_CALIB_SYMBOL
#define DEBUG_FIND_CALIB_SYMBOL
#include <iostream>
#include <opencv2/imgcodecs.hpp>
static const std::string DEBUG_RESULT_FOLDER = "/var/tmp/water/";
#endif

static const double MIN_SYMBOL_CONTOUR_SIZE = 50;
static const double MIN_SYMBOL_CONTOUR_AREA = 1500;
static const int MIN_SYMBOL_CONTOUR_LENGTH = 7;
static const double MAX_SYMBOL_CONTOUR_ELONG = 1.5;
using namespace cv;
using namespace std;

namespace gc
{

static double elongation( Moments m );
static double EuclidianDistance( Point a, Point b );

FindSymbol::FindSymbol()
{

}
// symbolPoints are clockwise ordered with 0 being the topmost left point
GC_STATUS FindSymbol::Find( const cv::Mat &img, std::vector< cv::Point2d > &symbolPoints )
{
    std::vector< SymbolCandidate > candidates;

    cv::Mat1b mask;
    GC_STATUS retVal = FindRed( img, mask, candidates );
    if ( GC_OK == retVal )
    {
        SymbolOctagonLines octoLines;
        Point2d ptTopLft, ptTopRgt, ptBotLft, ptBotRgt;
        for ( size_t i = 0; i < candidates.size(); ++i )
        {
            SymbolOctagonLines octoLines;
            retVal = FindCorners( mask, candidates[ i ].contour, octoLines );
            if ( GC_OK == retVal )
            {
                vector< Point > corners;
                retVal = FindDiagonals( mask, candidates[ i ].contour, octoLines );
                if ( GC_OK == retVal )
                {
                    retVal = CalcCorners( octoLines, symbolPoints );
                    if ( GC_OK == retVal )
                    {
#ifdef DEBUG_FIND_CALIB_SYMBOL
                        Mat color;
                        img.copyTo( color );
                        for ( size_t i = 0; i < symbolPoints.size(); ++i )
                        {
                            line( color, Point( symbolPoints[ i ].x - 10, symbolPoints[ i ].y ),
                                         Point( symbolPoints[ i ].x + 10, symbolPoints[ i ].y ),
                                  Scalar( 0, 255, 255 ), 1 );
                            line( color, Point( symbolPoints[ i ].x, symbolPoints[ i ].y - 10 ),
                                         Point( symbolPoints[ i ].x, symbolPoints[ i ].y + 10 ),
                                  Scalar( 0, 255, 255 ), 1 );
                        }
                        imwrite( DEBUG_RESULT_FOLDER + "___FINAL.png", color );
#endif
                    }
                }
            }
        }
    }

    return retVal;
}
GC_STATUS FindSymbol::FindRed( const cv::Mat &img, cv::Mat1b &redMask, std::vector< SymbolCandidate > &symbolCandidates )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        if ( img.type() != CV_8UC3 )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::FindRed] Image must be an 8-bit BGR image to find red";
            retVal = GC_ERR;
        }
        else
        {
            Mat3b hsv;
            cvtColor( img, hsv, COLOR_BGR2HSV );

            Mat1b mask1, mask2;
            inRange( hsv, Scalar( 0, 70, 50 ), Scalar( 10, 255, 255 ), mask1 );
            inRange( hsv, Scalar( 170, 70, 50 ), Scalar( 180, 255, 255 ), mask2 );

            redMask = mask1 | mask2;

            vector< vector< Point > > contours;
            findContours( redMask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE );

#ifdef DEBUG_FIND_CALIB_SYMBOL
            Mat color;
            img.copyTo( color );
            imwrite( DEBUG_RESULT_FOLDER + "red_mask.png", redMask );
#endif

            Moments m;
            double area, elong;
            symbolCandidates.clear();
            for ( size_t i = 0; i < contours.size(); ++i )
            {
                if ( MIN_SYMBOL_CONTOUR_LENGTH <= contours[ i ].size() )
                {
                    area = contourArea( contours[ i ] );
                    if ( MIN_SYMBOL_CONTOUR_AREA <= area )
                    {
                        m = moments( contours[ i ] );
                        elong = elongation( m );
                        cout << "elongation=" << elong << endl;
                        if ( MAX_SYMBOL_CONTOUR_ELONG >= elong )
                        {
                            symbolCandidates.push_back( SymbolCandidate( contours[ i ], area, elong ) );
#ifdef DEBUG_FIND_CALIB_SYMBOL
                            drawContours( color, contours, i, Scalar( 0, 255, 255 ), 3 );
#endif
                        }
                    }
                }
            }
            if ( symbolCandidates.empty() )
            {
                FILE_LOG( logERROR ) << "[FindSymbol::FindRed] No symbol candidates found";
                retVal = GC_ERR;
            }
#ifdef DEBUG_FIND_CALIB_SYMBOL
            else
            {
                imwrite( DEBUG_RESULT_FOLDER + "candidates.png", color );
            }
#endif
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::FindRed] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::FindCorners( const cv::Mat &mask, const std::vector< cv::Point > &contour, SymbolOctagonLines &octoLines )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        if ( contour.size() < MIN_SYMBOL_CONTOUR_SIZE )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::FindSymbolCorners] Contour must have at least " << MIN_SYMBOL_CONTOUR_SIZE << " contour points";
            retVal = GC_ERR;
        }
        else if ( mask.empty() || CV_8UC1 != mask.type() )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::FindSymbolCorners] Invalid mask image";
            retVal = GC_ERR;
        }
        else
        {
            Mat edges = Mat::zeros( mask.size(), CV_8UC1 );
            drawContours( edges, vector< vector< Point > >( 1, contour ), -1, Scalar( 255 ), 1 );
#ifdef DEBUG_FIND_CALIB_SYMBOL
            Mat color;
            cvtColor( mask, color, COLOR_GRAY2BGR );
            imwrite( DEBUG_RESULT_FOLDER + "candidate_contour.png", edges );
#endif
            Rect bb = boundingRect( contour );
            int swathSize = bb.height / 5;
            RotatedRect rotRect = fitEllipse( contour );
            Mat scratch = Mat::zeros( mask.size(), CV_8UC1 );
            line( scratch, rotRect.center, Point( 0, rotRect.center.y ), Scalar( 255 ), swathSize );
            scratch &= edges;
#ifdef DEBUG_FIND_CALIB_SYMBOL
            imwrite( DEBUG_RESULT_FOLDER + "left_edge_pts_swath.png", scratch );
#endif

            int top = rotRect.center.y - swathSize / 2;
            top = 0 > top ? 0 : top;
            int bot = rotRect.center.y + swathSize / 2;
            bot = scratch.rows <= bot ? scratch.rows - 1 : bot;

            Rect rect( 0, top, rotRect.center.x, bot - top );
            Point2d lftPt1, lftPt2;
            retVal = GetLineEndPoints( scratch, rect, lftPt1, lftPt2 );
            if ( GC_OK == retVal )
            {
                scratch = 0;
                line( scratch, rotRect.center, Point( scratch.cols - 1, rotRect.center.y ), Scalar( 255 ), swathSize );
                scratch &= edges;

                rect = Rect( rotRect.center.x, top, scratch.cols - rotRect.center.x, bot - top );
                Point2d rgtPt1, rgtPt2;
                retVal = GetLineEndPoints( scratch, rect, rgtPt1, rgtPt2 );
                if ( GC_OK == retVal )
                {
                    scratch = 0;
                    line( scratch, rotRect.center, Point( rotRect.center.x, 0 ), Scalar( 255 ), swathSize );
                    scratch &= edges;

                    int lft = rotRect.center.x - swathSize / 2;
                    lft = 0 > lft ? 0 : lft;
                    int rgt = rotRect.center.x + swathSize / 2;
                    rgt = scratch.cols <= rgt ? scratch.cols - 1 : rgt;

                    rect = Rect( lft, 0, rgt - lft, rotRect.center.y );
                    Point2d topPt1, topPt2;
                    retVal = GetLineEndPoints( scratch, rect, topPt1, topPt2 );
                    if ( GC_OK == retVal )
                    {
                        scratch = 0;
                        line( scratch, rotRect.center, Point( rotRect.center.x, scratch.rows - 1 ), Scalar( 255 ), swathSize );
                        scratch &= edges;

                        rect = Rect( lft, rotRect.center.y, rgt - lft, scratch.rows - rotRect.center.y );
                        Point2d botPt1, botPt2;
                        retVal = GetLineEndPoints( scratch, rect, botPt1, botPt2 );
                        if ( GC_OK == retVal )
                        {
#ifdef DEBUG_FIND_CALIB_SYMBOL
                            line( color, lftPt1, lftPt2, Scalar( 0, 0, 255 ), 1 );
                            line( color, rgtPt1, rgtPt2, Scalar( 0, 0, 255 ), 1 );
                            line( color, topPt1, topPt2, Scalar( 0, 0, 255 ), 1 );
                            line( color, botPt1, botPt2, Scalar( 0, 0, 255 ), 1 );
#endif
                            retVal = LineIntersection( SymbolLine( topPt1, topPt2 ), SymbolLine( lftPt1, lftPt2 ), octoLines.top.pt1 );
                            if ( GC_OK == retVal )
                            {
                                octoLines.left.pt2 = octoLines.top.pt1;
                                retVal = LineIntersection( SymbolLine( topPt1, topPt2 ), SymbolLine( rgtPt1, rgtPt2 ), octoLines.top.pt2 );
                                if ( GC_OK == retVal )
                                {
                                    octoLines.right.pt1 = octoLines.top.pt2;
                                    retVal = LineIntersection( SymbolLine( botPt1, botPt2 ), SymbolLine( lftPt1, lftPt2 ), octoLines.bot.pt2 );
                                    if ( GC_OK == retVal )
                                    {
                                        octoLines.left.pt1 = octoLines.bot.pt2;
                                        retVal = LineIntersection( SymbolLine( botPt1, botPt2 ), SymbolLine( rgtPt1, rgtPt2 ), octoLines.right.pt2 );
                                        if ( GC_OK == retVal )
                                        {
                                            octoLines.bot.pt1 = octoLines.right.pt2;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
#ifdef DEBUG_FIND_CALIB_SYMBOL
            imwrite( DEBUG_RESULT_FOLDER + "symbol_edges.png", color );
#endif
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::FindSymbolCorners] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::CalcCorners( const SymbolOctagonLines octoLines, std::vector< cv::Point2d > &symbolCorners )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        Point2d pt;
        symbolCorners.clear();
        retVal = LineIntersection( octoLines.topLeft, octoLines.top, pt );
        if ( GC_OK == retVal )
        {
            symbolCorners.push_back( pt );
            retVal = LineIntersection( octoLines.top, octoLines.topRight, pt );
            if ( GC_OK == retVal )
            {
                symbolCorners.push_back( pt );
                retVal = LineIntersection( octoLines.topRight, octoLines.right, pt );
                if ( GC_OK == retVal )
                {
                    symbolCorners.push_back( pt );
                    retVal = LineIntersection( octoLines.right, octoLines.botRight, pt );
                    if ( GC_OK == retVal )
                    {
                        symbolCorners.push_back( pt );
                        retVal = LineIntersection( octoLines.botRight, octoLines.bot, pt );
                        if ( GC_OK == retVal )
                        {
                            symbolCorners.push_back( pt );
                            retVal = LineIntersection( octoLines.bot, octoLines.botLeft, pt );
                            if ( GC_OK == retVal )
                            {
                                symbolCorners.push_back( pt );
                                retVal = LineIntersection( octoLines.botLeft, octoLines.left, pt );
                                if ( GC_OK == retVal )
                                {
                                    symbolCorners.push_back( pt );
                                    retVal = LineIntersection( octoLines.left, octoLines.topLeft, pt );
                                    if ( GC_OK == retVal )
                                    {
                                        symbolCorners.push_back( pt );
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::CalcCorners] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
// Finds the intersection of two lines, or returns false.
// The lines are defined by (o1, p1) and (o2, p2).
GC_STATUS FindSymbol::LineIntersection( const SymbolLine line1, const SymbolLine line2, Point2d &r )
{
    GC_STATUS retVal = GC_OK;
    try
    {
        Point2d x = line2.pt1 - line1.pt1;
        Point2d d1 = line1.pt2 - line1.pt1;
        Point2d d2 = line2.pt2 - line2.pt1;

        double cross = d1.x * d2.y - d1.y * d2.x;
        if (abs(cross) < numeric_limits< double >::epsilon() )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::LineIntersection] Lines are parallel";
            return GC_ERR;
        }

        double t1 = ( x.x * d2.y - x.y * d2.x ) / cross;
        r = line1.pt1 + d1 * t1;
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::LineIntersection] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::FindDiagonals( const cv::Mat &mask, const std::vector< cv::Point > &contour, SymbolOctagonLines &octoLines )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        if ( contour.size() < MIN_SYMBOL_CONTOUR_SIZE )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::FindSymbolCorners] Contour must have at least " << MIN_SYMBOL_CONTOUR_SIZE << " contour points";
            retVal = GC_ERR;
        }
        else if ( mask.empty() || CV_8UC1 != mask.type() )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::FindSymbolCorners] Invalid mask image";
            retVal = GC_ERR;
        }
        else
        {
            Mat edges = Mat::zeros( mask.size(), CV_8UC1 );
            drawContours( edges, vector< vector< Point > >( 1, contour ), -1, Scalar( 255 ), 1 );
#ifdef DEBUG_FIND_CALIB_SYMBOL
            Mat color;
            cvtColor( mask, color, COLOR_GRAY2BGR );
            imwrite( DEBUG_RESULT_FOLDER + "candidate_contour.png", edges );
#endif
            Rect bb = boundingRect( contour );
            int swathSize = bb.height / 5;
            RotatedRect rotRect = fitEllipse( contour );
            Mat scratch = Mat::zeros( mask.size(), CV_8UC1 );
            line( scratch, rotRect.center, octoLines.top.pt1, Scalar( 255 ), swathSize );
            scratch &= edges;
#ifdef DEBUG_FIND_CALIB_SYMBOL
            imwrite( DEBUG_RESULT_FOLDER + "top_left_edge_pts_swath.png", scratch );
#endif

            Rect rect( octoLines.top.pt1.x, octoLines.top.pt1.y, rotRect.center.x - octoLines.top.pt1.x,
                       rotRect.center.y - octoLines.top.pt1.y );

            retVal = GetLineEndPoints( scratch, rect, octoLines.topLeft.pt1, octoLines.topLeft.pt2 );
            if ( GC_OK == retVal )
            {
                scratch = 0;
                line( scratch, rotRect.center, octoLines.top.pt2, Scalar( 255 ), swathSize );
                scratch &= edges;
#ifdef DEBUG_FIND_CALIB_SYMBOL
                imwrite( DEBUG_RESULT_FOLDER + "top_right_edge_pts_swath.png", scratch );
#endif

                rect = Rect( rotRect.center.x, octoLines.top.pt2.y, octoLines.top.pt2.x - rotRect.center.x,
                             rotRect.center.y - octoLines.top.pt2.y );

                retVal = GetLineEndPoints( scratch, rect, octoLines.topRight.pt1, octoLines.topRight.pt2 );
                if ( GC_OK == retVal )
                {
                    scratch = 0;
                    line( scratch, rotRect.center, octoLines.bot.pt2, Scalar( 255 ), swathSize );
                    scratch &= edges;
#ifdef DEBUG_FIND_CALIB_SYMBOL
                    imwrite( DEBUG_RESULT_FOLDER + "bot_left_edge_pts_swath.png", scratch );
#endif

                    rect = Rect( octoLines.bot.pt2.x, rotRect.center.y, rotRect.center.x - octoLines.bot.pt2.x,
                                 octoLines.bot.pt2.y - rotRect.center.y );
                    retVal = GetLineEndPoints( scratch, rect, octoLines.botLeft.pt1, octoLines.botLeft.pt2 );
                    if ( GC_OK == retVal )
                    {
                        scratch = 0;
                        line( scratch, rotRect.center, octoLines.bot.pt1, Scalar( 255 ), swathSize );
                        scratch &= edges;
#ifdef DEBUG_FIND_CALIB_SYMBOL
                        imwrite( DEBUG_RESULT_FOLDER + "bot_right_edge_pts_swath.png", scratch );
#endif

                        rect = Rect( rotRect.center.x, rotRect.center.y, octoLines.bot.pt1.x - rotRect.center.x,
                                     octoLines.bot.pt1.y - rotRect.center.y );

                        retVal = GetLineEndPoints( scratch, rect, octoLines.botRight.pt1, octoLines.botRight.pt2 );
                        if ( GC_OK == retVal )
                        {
#ifdef DEBUG_FIND_CALIB_SYMBOL
                            line( color, octoLines.topLeft.pt1, octoLines.topLeft.pt2, Scalar( 0, 0, 255 ), 1 );
                            line( color, octoLines.topRight.pt1, octoLines.topRight.pt2, Scalar( 0, 0, 255 ), 1 );
                            line( color, octoLines.botLeft.pt1, octoLines.botLeft.pt2, Scalar( 0, 0, 255 ), 1 );
                            line( color, octoLines.botRight.pt1, octoLines.botRight.pt2, Scalar( 0, 0, 255 ), 1 );
#endif
                        }
                    }
                }
            }
#ifdef DEBUG_FIND_CALIB_SYMBOL
            imwrite( DEBUG_RESULT_FOLDER + "symbol_edges_diagonal.png", color );
#endif
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::FindDiagonals] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::GetLineEndPoints( cv::Mat &mask, const cv::Rect rect, cv::Point2d &pt1, cv::Point2d &pt2 )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        Mat search = mask( rect );
#ifdef DEBUG_FIND_CALIB_SYMBOL
        imwrite( DEBUG_RESULT_FOLDER + "pt_search_img.png", mask );
        imwrite( DEBUG_RESULT_FOLDER + "pt_search_rect.png", search );
#endif

        vector< Point > pts;
        retVal = GetNonZeroPoints( search, pts );
        if ( GC_OK == retVal )
        {
            for ( size_t i = 0; i < pts.size(); ++i )
            {
                pts[ i ].x += rect.x;
                pts[ i ].y += rect.y;
            }
#ifdef DEBUG_FIND_CALIB_SYMBOL
            Mat color;
            cvtColor( mask, color, COLOR_GRAY2BGR );
            drawContours( color, vector< vector< Point > >( 1, pts ), -1, Scalar( 0, 255, 255 ), 1 );
            imwrite( DEBUG_RESULT_FOLDER + "pt_search_pts.png", color );
#endif

            Vec4d lne;
            fitLine( pts, lne, DIST_L12, 0.0, 0.01, 0.01 );

            // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            // line equation: y = mx + b
            // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            Point2d pt1x0, pt1y0, pt2x0, pt2y0;

            double a = lne[ 1 ];
            double b = -lne[ 0 ];
            double c = lne[ 0 ] * lne[ 3 ] - lne[ 1 ] * lne[ 2 ];

            double denom = ( 0.0 == a ? std::numeric_limits< double >::epsilon() : a );
            pt1y0.y = 0.0;
            pt1y0.x = c / -denom;
            pt2y0.y = static_cast< double >( mask.rows - 1 );
            pt2y0.x = ( b * pt2y0.y + c ) / -denom;

            denom = ( 0.0 == b ? std::numeric_limits< double >::epsilon() : b );
            pt1x0.x = 0.0;
            pt1x0.y = c / -denom;
            pt2x0.x = static_cast< double >( mask.cols - 1 );
            pt2x0.y = ( a * pt2x0.x + c ) / -denom;

            if ( 0.0 <= pt1y0.x && 0.0 <= pt1y0.y && mask.cols > pt1y0.x && mask.rows > pt1y0.y )
            {
                pt1 = pt1y0;
            }
            else
            {
                pt1 = pt1x0;
            }


            if ( 0.0 <= pt2y0.x && 0.0 <= pt2y0.y && mask.cols > pt2y0.x && mask.rows > pt2y0.y )
            {
                pt2 = pt2y0;
            }
            else
            {
                pt2 = pt2x0;
            }
#ifdef DEBUG_FIND_CALIB_SYMBOL
            line( color, pt1, pt2, Scalar( 0, 255, 0 ), 1 );
            imwrite( DEBUG_RESULT_FOLDER + "pt_search_line.png", color );
#endif
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::GetLineEndPoints] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;

}
GC_STATUS FindSymbol::GetNonZeroPoints( cv::Mat &img, std::vector< cv::Point > &pts )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        if ( img.empty() )
        {
            FILE_LOG( logERROR ) << "[FindSymbol::GetNonZeroPoints] Can not get points from an empty image";
            retVal = GC_ERR;
        }
        else
        {
            pts.clear();
            uchar *pPix;
            for( int r = 0; r < img.rows; ++r )
            {
                pPix = img.ptr< uchar >( r );
                for ( int c = 0; c < img.cols; ++c )
                {
                    if ( 0 != pPix[ c ] )
                        pts.push_back( Point( c, r ) );
                }
            }
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::GetNonZeroPoints] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::FindCenter( const Mat &img, SymbolMatch &matchResult )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        Mat searchImg;
        Mat matchSpace;
        double dMin, dMax;
        Point ptMin, ptMax;
        double sizeRatio = static_cast< double >( img.rows ) / static_cast< double >( img.cols );
        for ( int w = SYMBOL_SEARCH_IMAGE_WIDTH_START; w <= SYMBOL_SEARCH_IMAGE_WIDTH_END; w += SYMBOL_SEARCH_IMAGE_WIDTH_INC )
        {
            resize( img, searchImg, Size( w, cvRound( static_cast< double >( w ) * sizeRatio ) ), 0.0, 0.0, INTER_CUBIC );
            for ( size_t i = 0; i < symbolTemplates.size(); ++i )
            {
                matchSpace = Mat();
                matchTemplate( searchImg, symbolTemplates[ i ], matchSpace, cv::TM_CCOEFF_NORMED );
                minMaxLoc( matchSpace, &dMin, &dMax, &ptMin, &ptMax );
                if ( dMax >= matchResult.score )
                {
                    ptMax.x += symbolTemplates[ i ].cols / 2;
                    ptMax.y += symbolTemplates[ i ].rows / 2;
                    matchResult = SymbolMatch( ptMax, dMax, w, i );
                }
            }
        }
        double scale = static_cast< double >( img.cols ) / static_cast< double >( matchResult.width );
        matchResult.pt = Point( cvRound( static_cast< double >( matchResult.pt.x ) * scale ), cvRound( static_cast< double >( matchResult.pt.y ) *scale ) );

#ifdef DEBUG_FIND_CALIB_SYMBOL   // debug of template rotation
        std::cout << "Score=" << matchResult.score << " x=" << matchResult.pt.x << " y=" << matchResult.pt.y;
        std::cout << " angleIndex=" << matchResult.angleIdx << " scale=" << scale << std::endl;
        Mat color;
        cvtColor( img, color, COLOR_GRAY2BGR );
        circle( color, matchResult.pt, 25, Scalar( 255, 0, 0 ), 5 );
        line( color, Point( matchResult.pt.x - 25, matchResult.pt.y ), Point( matchResult.pt.x + 25, matchResult.pt.y ), Scalar( 0, 255, 255 ), 3 );
        line( color, Point( matchResult.pt.x, matchResult.pt.y - 25 ), Point( matchResult.pt.x, matchResult.pt.y + 25 ), Scalar( 0, 255, 255 ), 3 );
        imwrite( DEBUG_RESULT_FOLDER + "sign_center.png", color );
#endif
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::FindCenter] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::CreateSymbolTemplates( const cv::Mat &refTemplate )
{
    GC_STATUS retVal = GC_OK;

    try
    {
        // create templates
        symbolTemplates.clear();

        Mat refAdj;
        double sizeRatio = static_cast< double >( SYMBOL_TEMPL_WIDTH ) / static_cast< double >( refTemplate.cols );
        Size sizeAdj( SYMBOL_TEMPL_WIDTH, cvRound( sizeRatio * static_cast< double >( refTemplate.rows ) ) );
        resize( refTemplate, refAdj, sizeAdj, 0.0, 0.0, INTER_CUBIC );

        int templateCount = cvRound( 2.0 * ( SYMBOL_TEMPL_ANGLE_MAX / SYMBOL_TEMPL_ANGLE_INC ) ) + 1;

        int tempWidth = refAdj.cols << 1;
        int tempHeight = refAdj.rows << 1;
        Rect roiRotate( tempWidth >> 2, tempHeight >> 2, refAdj.cols, refAdj.rows );
        Mat matTemp = Mat::zeros( Size( tempWidth, tempHeight ), CV_8U );
        refAdj.copyTo( matTemp( roiRotate ) );
        Mat matTempRot( Size( tempWidth, tempHeight ), CV_8U );

        for ( int i = 0; i < templateCount; ++i )
            symbolTemplates.push_back( Mat::zeros( sizeAdj, CV_8U ) );

        // create the rotated templates
        int center = templateCount / 2;

        Mat matTemplateTemp = matTemp( roiRotate );

        matTemplateTemp.copyTo( symbolTemplates[ static_cast< size_t >( center ) ] );
#ifdef DEBUG_FIND_CALIB_SYMBOL   // debug of template rotation
        char msg[ 1024 ];
        sprintf( msg, "%02d_template_center.png", center );
        imwrite( DEBUG_RESULT_FOLDER + msg, symbolTemplates[ static_cast< size_t >( center ) ] );
        imwrite( DEBUG_RESULT_FOLDER + "ref_template.png", refTemplate );
#endif

        for ( int i = 0; i < center; ++i )
        {
            retVal = RotateImage( matTemp, matTempRot, static_cast< double >( i - center ) );
            if ( GC_OK != retVal )
                break;

            matTemplateTemp = matTempRot( roiRotate );
            matTemplateTemp.copyTo( symbolTemplates[ static_cast< size_t >( i ) ] );

#ifdef DEBUG_FIND_CALIB_SYMBOL   // debug of template rotation
            sprintf( msg, "%02d_template.png", i );
            imwrite( DEBUG_RESULT_FOLDER + msg, symbolTemplates[ static_cast< size_t >( i ) ] );
#endif
            retVal = RotateImage( matTemp, matTempRot, static_cast< double >( i + 1 ) );
            if ( 0 != retVal )
                break;

            matTemplateTemp = matTempRot( roiRotate );
            matTemplateTemp.copyTo( symbolTemplates[ static_cast< size_t >( center + i + 1 ) ] );
#ifdef DEBUG_FIND_CALIB_SYMBOL   // debug of template rotation
            sprintf( msg, "%02d_template.png", center + i + 1 );
            imwrite( DEBUG_RESULT_FOLDER + msg, symbolTemplates[ static_cast< size_t >( center + i + 1 ) ] );
#endif
        }
    }
    catch( cv::Exception &e )
    {
        FILE_LOG( logERROR ) << "[FindSymbol::CreateSymbolTemplates] " << e.what();
        retVal = GC_EXCEPT;
    }

    return retVal;
}
GC_STATUS FindSymbol::RotateImage( const Mat &src, Mat &dst, const double angle )
{
    GC_STATUS retVal = GC_OK;
    try
    {
        Point2d ptCenter = Point2d( static_cast< double >( src.cols ) / 2.0, static_cast< double >( src.rows ) / 2.0 );
        Mat matRotMatrix = getRotationMatrix2D( ptCenter, angle, 1.0 );
        warpAffine( src, dst, matRotMatrix, dst.size(), INTER_CUBIC );
    }
    catch( Exception &e )
    {
        FILE_LOG( logERROR ) << "[RotateImage::RotateImage] " << e.what();
        retVal = GC_EXCEPT;
    }
    return retVal;
}

// helper functions
static double EuclidianDistance( Point a, Point b )
{
    return sqrt( ( b.x - a.x ) * ( b.x - a.x ) + ( b.y - a.y ) * ( b.y - a.y ) );
}
static double elongation( Moments m )
{
    double x = m.mu20 + m.mu02;
    double y = 4 * m.mu11 * m.mu11 + ( m.mu20 - m.mu02 ) * ( m.mu20 - m.mu02 );
    double srt = sqrt( y );
    if ( x - srt > DBL_EPSILON )
        return ( x + srt ) / ( x - srt );
    else
        return 1.0;
}

} // namespace gc
