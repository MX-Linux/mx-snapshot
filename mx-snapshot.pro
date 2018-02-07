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

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mx-snapshot
TEMPLATE = app


SOURCES += main.cpp\
        mxsnapshot.cpp

HEADERS  += mxsnapshot.h

FORMS    += mxsnapshot.ui

TRANSLATIONS += translations/mx-bootrepair_am.ts \
                translations/mx-bootrepair_ca.ts \
                translations/mx-bootrepair_cs.ts \
                translations/mx-bootrepair_de.ts \
                translations/mx-bootrepair_el.ts \
                translations/mx-bootrepair_es.ts \
                translations/mx-bootrepair_fr.ts \
                translations/mx-bootrepair_hu.ts \
                translations/mx-bootrepair_it.ts \
                translations/mx-bootrepair_ja.ts \
                translations/mx-bootrepair_kk.ts \
                translations/mx-bootrepair_lt.ts \
                translations/mx-bootrepair_nl.ts \
                translations/mx-bootrepair_pl.ts \
                translations/mx-bootrepair_pt.ts \
                translations/mx-bootrepair_pt_BR.ts \
                translations/mx-bootrepair_ro.ts \
                translations/mx-bootrepair_ru.ts \
                translations/mx-bootrepair_sk.ts \
                translations/mx-bootrepair_sv.ts \
                translations/mx-bootrepair_tr.ts \
                translations/mx-bootrepair_uk.ts

RESOURCES += \
    images.qrc


