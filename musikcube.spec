%define name musikcube
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}
%define version 0.31.0
Name: %{name}           
Version: %{version}     
Release: %{build_timestamp}%{dist}
Summary: A cross-platform, terminal-based audio engine, library, player and server written in C++
Source0: https://github.com/clangen/musikcube/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz
License: GPLv3
Packager: David Muckle <dvdmuckle@dvdmuckle.xyz>
BuildRequires: gcc-c++ cmake boost-devel libogg-devel libvorbis-devel flac-devel faad2-devel ncurses-devel zlib-devel alsa-lib-devel pulseaudio-libs-devel libcurl-devel libmicrohttpd-devel lame-devel
Requires: boost libogg libvorbis flac ncurses zlib alsa-lib pulseaudio-libs libcurl libmicrohttpd lame
Recommends: faad2

%description

A cross-platform, terminal-based audio engine, library, player and server written in C++
%global debug_package %{nil}
%prep
%autosetup -n %{name}-%{version}


%build
#cmake -DCMAKE_INSTALL_PREFIX:PATH=%{buildroot}%{_prefix} .
cmake .
make -j2


%install
mkdir %{buildroot}%{_prefix} -p
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%{_prefix}/local/bin/musikcube
%{_prefix}/local/share/musikcube/
%{_prefix}/local/include/musikcube/
%doc
