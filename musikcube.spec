%define name musikcube
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}
%define version 0.51.0
Name: %{name}
Version: %{version}
Release: %{dist}
Summary: A cross-platform, terminal-based audio engine, library, player and server written in C++
Source0: https://github.com/clangen/musikcube/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz
License: BSD-3-Clause
Packager: David Muckle <dvdmuckle@dvdmuckle.xyz>
BuildRequires: gcc-c++ cmake boost-devel libogg-devel libvorbis-devel flac-devel faad2-devel ncurses-devel zlib-devel alsa-lib-devel pulseaudio-libs-devel openssl-devel libcurl-devel libmicrohttpd-devel lame-devel libev-devel make
Requires: boost libogg libvorbis flac ncurses zlib alsa-lib pulseaudio-libs openssl libcurl libmicrohttpd lame libev
Recommends: faad2

%description

A cross-platform, terminal-based audio engine, library, player and server written in C++
%global debug_package %{nil}
%prep
%autosetup -n %{name}-%{version}

%build
cmake -D CMAKE_INSTALL_PREFIX:PATH=%{_prefix} -DCMAKE_BUILD_TYPE=Release .
make -j2

%install
make install DESTDIR=%{buildroot}
find %{buildroot} -type f \( -name '*.so' -o -name '*.so.*' \) -exec chmod 755 {} +

%clean
rm -rf %{buildroot}

%files
%{_prefix}/bin/musikcube
%{_prefix}/bin/musikcubed
%{_prefix}/share/musikcube/
%{_prefix}/include/musikcube/
%doc
