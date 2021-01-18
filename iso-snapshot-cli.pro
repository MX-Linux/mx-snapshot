# **********************************************************************
# * Copyright (C) 2015 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of MX Snapshot.
# *
# * MX Snapshot is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * MX Snapshot is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with MX Tools.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************

#-------------------------------------------------
#
# Project created by QtCreator 2014-12-14
#
#-------------------------------------------------

QT       += core

CONFIG   += c++11

#greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = iso-snapshot-cli
TEMPLATE = app


SOURCES += maincli.cpp \
    cmd.cpp \
    settings.cpp \
    batchprocessing.cpp \
    work.cpp

HEADERS  += \
    version.h \
    cmd.h \
    settings.h \
    batchprocessing.h \
    work.h

TRANSLATIONS += translations/mx-snapshot_am.ts \
                translations/mx-snapshot_ar.ts \
                translations/mx-snapshot_bg.ts \
                translations/mx-snapshot_bn.ts \
                translations/mx-snapshot_ca.ts \
                translations/mx-snapshot_cs.ts \
                translations/mx-snapshot_da.ts \
                translations/mx-snapshot_de.ts \
                translations/mx-snapshot_el.ts \
                translations/mx-snapshot_es.ts \
                translations/mx-snapshot_et.ts \
                translations/mx-snapshot_eu.ts \
                translations/mx-snapshot_fa.ts \
                translations/mx-snapshot_fi.ts \
                translations/mx-snapshot_fil_PH.ts \
                translations/mx-snapshot_fr.ts \
                translations/mx-snapshot_he_IL.ts \
                translations/mx-snapshot_hi.ts \
                translations/mx-snapshot_hr.ts \
                translations/mx-snapshot_hu.ts \
                translations/mx-snapshot_id.ts \
                translations/mx-snapshot_is.ts \
                translations/mx-snapshot_it.ts \
                translations/mx-snapshot_ja.ts \
                translations/mx-snapshot_kk.ts \
                translations/mx-snapshot_ko.ts \
                translations/mx-snapshot_lt.ts \
                translations/mx-snapshot_mk.ts \
                translations/mx-snapshot_mr.ts \
                translations/mx-snapshot_nb.ts \
                translations/mx-snapshot_nl.ts \
                translations/mx-snapshot_pl.ts \
                translations/mx-snapshot_pt.ts \
                translations/mx-snapshot_pt_BR.ts \
                translations/mx-snapshot_ro.ts \
                translations/mx-snapshot_ru.ts \
                translations/mx-snapshot_sk.ts \
                translations/mx-snapshot_sl.ts \
                translations/mx-snapshot_sq.ts \
                translations/mx-snapshot_sr.ts \
                translations/mx-snapshot_sv.ts \
                translations/mx-snapshot_tr.ts \
                translations/mx-snapshot_uk.ts \
                translations/mx-snapshot_vi.ts \
                translations/mx-snapshot_zh_CN.ts \
                translations/mx-snapshot_zh_TW.ts

