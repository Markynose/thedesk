Name:           thedesk-xdg-utils-blueprint
Version:        0.1
Release:        1%{?dist}
Summary:        Helper for theDesk default applications

License:        GPLv3+
URL:            https://github.com/vicr123/thedesk-xdg-utils
Source0:        https://github.com/vicr123/thedesk-xdg-utils/archive/%{version}.tar.gz
Conflicts:      thedesk-xdg-utils
Provides:       thedesk-xdg-utils

%if 0%{?fedora} >= 33
BuildRequires:  make qt5-qtbase-devel the-libs-blueprint-devel libtdesktopenvironment-blueprint-devel qt5-linguist 
Requires:       qt5-qtbase the-libs-blueprint libtdesktopenvironment-blueprint
%endif

%define debug_package %{nil}
%define _unpackaged_files_terminate_build 0

%description
Helper for theDesk default applications

%prep
%setup

%build
mkdir build
cd build
qmake-qt5 ..
make

%install
cd build
rm -rf $RPM_BUILD_ROOT
make install INSTALL_ROOT=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_bindir}/*
/usr/lib/thedesk/xdg-utils/*

%changelog
* Sun May  3 2020 Victor Tran
- 
