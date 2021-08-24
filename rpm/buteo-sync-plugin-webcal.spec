Name:       buteo-sync-plugin-webcal
Summary:    Syncs online calendar resource in ICS format
Version:    0.1.9
Release:    1
License:    LGPLv2+
URL:        https://git.sailfishos.org/mer-core/buteo-sync-plugin-webcal
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(libmkcal-qt5) >= 0.5.9
BuildRequires:  pkgconfig(KF5CalendarCore)
BuildRequires:  pkgconfig(buteosyncfw5) >= 0.10.0
Requires: buteo-syncfw-qt5-msyncd

%description
A Buteo plugin which syncs a ICS resource online

%files
%defattr(-,root,root,-)
%license LICENSE
%{_libdir}/buteo-plugins-qt5/oopp/libwebcal-client.so
%config %{_sysconfdir}/buteo/profiles/sync/webcal-sync.xml
%config %{_sysconfdir}/buteo/profiles/client/webcal.xml

%package tests
Summary:  Unit tests for web calendar Buteo sync plugin
Requires:   %{name} = %{version}

%description tests
This package contains unit tests for web calendar Buteo sync plugin

%files tests
%defattr(-,root,root,-)
/opt/tests/buteo/plugins/webcal

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

%post
systemctl-user try-restart msyncd.service || :
