#! /bin/sh
rm -rf _build_fmiSmsCtrl
mkapp -i ../../interfaces -i ../../interfaces/modemServices -t wp85 fmiSmsCtrl.adef
