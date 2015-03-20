Summary: Queue with persistent disk storage.
Name: solid_queue
Version: 1.1.0.0
Release: 1%{?dist}
License: Proprietary
Group: Libraries
Source0: %{name}-%{version}.tar.gz
BuildArch: x86_64
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Solid_queue is a queue with on disk storage. 

BuildRequires:	cmake >= 2.8
BuildRequires:	eblob
BuildRequires:  pthread
BuildRequires:  rt

%prep
%setup -q

%build
cd %{_builddir}/%{name}-%{version}/
%{cmake} .
make -j %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/lib/
mkdir -p $RPM_BUILD_ROOT/usr/include/
cp %{_builddir}/%{name}-%{version}/src/libsolid_queue.so $RPM_BUILD_ROOT/usr/lib/
cp %{_builddir}/%{name}-%{version}/include/solid_queue.h $RPM_BUILD_ROOT/usr/include/

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
/usr/lib/libsolid_queue.so
/usr/include/solid_queue.h


%clean
rm -rf %{buildroot}

%changelog
* Tue Mar 10 2015 Dmitry Rudnev <rudneff.d@gmail.com> - 1.2.0.0
- init: added a pointer to a custom log handler
- init: added pointer to pass to log function as first argument

* Tue Dec 16 2014 Dmitry Rudnev <rudneff.d@gmail.com> - 1.1.0.0
- init: added error number returning to initial functions
- test: added some tests
- deb: added files for debian packaging

* Mon Oct 20 2014 Dmitry Rudnev <rudneff.d@gmail.com> - 1.0.0.0
- First release
