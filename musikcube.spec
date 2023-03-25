%define name musikcube
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}
%define version 0.99.8
Name: %{name}
Version: %{version}
Release: %{dist}
Summary: A cross-platform, terminal-based audio engine, library, player and server written in C++
Source0: https://github.com/clangen/musikcube/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz
License: BSD-3-Clause
Packager: David Muckle <dvdmuckle@dvdmuckle.xyz>
BuildRequires: gcc-c++ cmake libogg-devel libvorbis-devel ffmpeg-devel ncurses-devel zlib-devel alsa-lib-devel pulseaudio-libs-devel openssl-devel libcurl-devel libmicrohttpd-devel lame-devel libev-devel taglib-devel systemd-devel make libopenmpt-devel
Requires: libogg libvorbis ffmpeg-libs ncurses zlib alsa-lib pulseaudio-libs openssl libcurl libmicrohttpd lame libev taglib libopenmpt

%description

A cross-platform, terminal-based audio engine, library, player and server written in C++
%global debug_package %{nil}
%prep
%autosetup -n %{name}-%{version}

%build
cmake -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} -DCMAKE_BUILD_TYPE=Release -DENABLE_PCH=true -DENABLE_PIPEWIRE=$ENABLE_PIPEWIRE .
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
%{_prefix}/share/applications/
%{_prefix}/share/icons/
%{_prefix}/include/musikcube/
%doc
