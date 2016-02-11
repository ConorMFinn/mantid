#include "MantidQtSliceViewer/PeakRepresentation.h"

#include <QPainter>

namespace MantidQt
{
namespace SliceViewer
{

/**
 * Template method which draws a peaks representation
 * @param painter: a QPainter to draw peak information onto the screen
 * @paran viewInformation: information about the view into which the peak is drawn
 */
void PeakRepresentation::draw(QPainter& painter, QColor& peakColor, PeakRepresentationViewInformation viewInformation) {
  // Setup the drawing information, eg positions, radii ...
  auto drawingInformation = getDrawingInformation(viewInformation);
  // Draw
  doDraw(painter, peakColor, drawingInformation, viewInformation);
}


}
}
