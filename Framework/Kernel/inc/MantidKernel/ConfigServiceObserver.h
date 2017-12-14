#ifndef MANTID_KERNEL_CONFIGSERVICEOBSERVER_H_
#define MANTID_KERNEL_CONFIGSERVICEOBSERVER_H_
#include "ConfigService.h"
#include "Poco/NObserver.h"
#include "MantidKernel/DllConfig.h"

namespace Mantid {
namespace Kernel {
/** @class The ConfigServiceObserver provides a simpler way to observe
   ConfigService notifications.

    Copyright &copy; 2007-2010 ISIS Rutherford Appleton Laboratory, NScD Oak
   Ridge National Laboratory & European Spallation Source

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

    File change history is stored at: <https://github.com/mantidproject/mantid>.
    Code Documentation is available at: <http://doxygen.mantidproject.org>
 */
class MANTID_KERNEL_DLL ConfigServiceObserver {
public:
  ConfigServiceObserver();
  ConfigServiceObserver(const ConfigServiceObserver& other);
  ConfigServiceObserver(ConfigServiceObserver&& other) noexcept;
  ConfigServiceObserver& operator=(const ConfigServiceObserver& other);
  ConfigServiceObserver& operator=(ConfigServiceObserver&& other) noexcept;
  virtual ~ConfigServiceObserver() noexcept;

  void notifyValueChanged(const std::string &name, const std::string &newValue,
                          const std::string &prevValue);
  void notifyValueChanged(ConfigValChangeNotification_ptr notification);
protected:
  virtual void onValueChanged(const std::string &name,
                              const std::string &newValue,
                              const std::string &prevValue);

private:
  Poco::NObserver<ConfigServiceObserver,
                  Mantid::Kernel::ConfigValChangeNotification>
      m_valueChangeListener;
};
}
}
#endif /*MANTID_KERNEL_CONFIGSERVICEOBSERVER_H_*/
