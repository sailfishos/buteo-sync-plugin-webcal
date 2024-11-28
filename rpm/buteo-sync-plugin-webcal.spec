Name:       buteo-sync-plugin-webcal
Summary:    Syncs online calendar resource in ICS format
Version:    0.1.13
Release:    1
License:    LGPLv2+
URL:        https://github.com/sailfishos/buteo-sync-plugin-webcal
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(libmkcal-qt5) >= 0.6.10
BuildRequires:  pkgconfig(KF5CalendarCore)
BuildRequires:  pkgconfig(buteosyncfw5) >= 0.10.0
Requires: buteo-syncfw-qt5-msyncd

%description
A Buteo plugin which syncs a ICS resource online

%package tests
Summary:  Unit tests for web calendar Buteo sync plugin
Requires:   %{name} = %{version}

%description tests
This package contains unit tests for web calendar Buteo sync plugin

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

%post
systemctl-user try-restart msyncd.service || :

%files
%license LICENSE
%{_libdir}/buteo-plugins-qt5/oopp/libwebcal-client.so
%config %{_sysconfdir}/buteo/profiles/sync/webcal-sync.xml
%config %{_sysconfdir}/buteo/profiles/client/webcal.xml

%files tests
/opt/tests/buteo/plugins/webcal
