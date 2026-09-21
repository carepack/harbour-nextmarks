Name:       harbour-nextmarks

Summary:    Save links to Nextcloud Bookmarks
Version:    1.0.0
Release:    2
License:    MIT
URL:        https://github.com/carepack/harbour-nextmarks
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  desktop-file-utils
BuildRequires:  cmake

%description
Share a link from any app into your Nextcloud Bookmarks, in the folder of
your choice. Logs in via Nextcloud's own browser-based login flow -- no
sync, no local bookmark database, just a one-way "save this link" action.

%if 0%{?_chum}
Title: Nextmarks
Type: desktop-application
DeveloperName: harbour-nextmarks contributors
Categories:
 - Network
 - Utility
Custom:
  Repo: https://github.com/carepack/harbour-nextmarks
Links:
  Homepage: https://github.com/carepack/harbour-nextmarks
  Bugtracker: https://github.com/carepack/harbour-nextmarks/issues
%endif


%prep
%setup -q -n %{name}-%{version}

%build
%cmake
%make_build

%install
%make_install

desktop-file-install --delete-original \
    --dir %{buildroot}%{_datadir}/applications \
    %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/dbus-1/services/%{name}.%{name}.service
%{_datadir}/icons/hicolor/*/apps/%{name}.png
