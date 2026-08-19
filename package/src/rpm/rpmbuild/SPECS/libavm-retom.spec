Name:		libavm-retom
Version:    @VERSION@
Release:    1%{?dist}
Summary:    RETOM-51 test-bench driver library for AVM-Energo projects

License:    GPLv3
URL:        https://git.avmenergo.ru/avm-energo/libavm-retom.git

BuildArch:  @ARCH@
Provides:   libavm-retom

%description
RETOM-51 test-bench driver library for AVM-Energo projects

%package devel
Summary:    RETOM-51 test-bench driver library development files for AVM-Energo projects
Requires:   libavm-retom = %{version}-%{release}

%description devel
RETOM-51 test-bench driver library development files for AVM-Energo projects

%prep
# sources pre-built by cmake_build TeamCity step

%build
# build performed by cmake_build TeamCity step

%install
# files pre-installed to BUILDROOT by build_rpm TeamCity step

%files
%defattr(-,root,root)
/usr/lib64/libavm-retom.so*

%files devel
%defattr(-,root,root)
/usr/lib64/libavm-retom.a
/usr/lib64/cmake/libavm-retom/
/usr/include/libavm-retom/

%changelog
* Wed Aug 19 2026 AVM-Energo <support@avmenergo.ru>
- Initial release: connect/disconnect, enable/disable, apply/off/safety_stop, read_input_contacts/set_out_contact over FTDI VCP (RETOM-51)
