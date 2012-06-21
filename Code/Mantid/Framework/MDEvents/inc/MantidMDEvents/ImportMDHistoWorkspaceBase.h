#ifndef MANTID_MDEVENTS_IMPORTMDHISTOWORKSPACEBASE_H_
#define MANTID_MDEVENTS_IMPORTMDHISTOWORKSPACEBASE_H_

#include "MantidKernel/System.h"
#include "MantidAPI/Algorithm.h"
#include "MantidMDEvents/MDHistoWorkspace.h"
#include <vector>

namespace Mantid
{
namespace MDEvents
{

  /** ImportMDHistoWorkspaceBase : Base class for algorithms Importing data as MDHistoWorkspaces.
    
    @date 2012-06-21

    Copyright &copy; 2012 ISIS Rutherford Appleton Laboratory & NScD Oak Ridge National Laboratory

    This file is part of Mantid.

    Mantid is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Mantid is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    File change history is stored at: <https://svn.mantidproject.org/mantid/trunk/Code/Mantid>
    Code Documentation is available at: <http://doxygen.mantidproject.org>
  */
  class DLLExport ImportMDHistoWorkspaceBase  : public API::Algorithm
  {
  public:

    ImportMDHistoWorkspaceBase();
    virtual ~ImportMDHistoWorkspaceBase();
    
  protected:
    /// Vector containing the number of bins in each dimension.
    std::vector<int> nbins;
    /// Creates an empty md histo workspace (with dimensions)
    MDHistoWorkspace_sptr ImportMDHistoWorkspaceBase::createEmptyOutputWorkspace();
    /// Initialise the properties associated with the generic import (those to do with dimensionality).
    void initGenericImportProps();
    /// Getter for the number of bins (product accross all dimensions)
    size_t getBinProduct() const
    {
      return m_bin_product;
    }

  private:
    // Product of the bins across all dimensions.
    size_t m_bin_product;
  };


} // namespace MDEvents
} // namespace Mantid

#endif  /* MANTID_MDEVENTS_IMPORTMDHISTOWORKSPACEBASE_H_ */