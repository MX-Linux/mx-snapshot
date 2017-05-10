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

TRANSLATIONS += translations/mx-snapshot_ca.ts \
                translations/mx-snapshot_cs.ts \
                translations/mx-snapshot_de.ts \
                translations/mx-snapshot_el.ts \
                translations/mx-snapshot_es.ts \
                translations/mx-snapshot_fr.ts \
                translations/mx-snapshot_it.ts \
                translations/mx-snapshot_ja.ts \
                translations/mx-snapshot_lt.ts \
                translations/mx-snapshot_nl.ts \
                translations/mx-snapshot_pl.ts \
                translations/mx-snapshot_pt.ts \
                translations/mx-snapshot_ro.ts \
                translations/mx-snapshot_ru.ts \
                translations/mx-snapshot_sv.ts \
                translations/mx-snapshot_tr.ts

RESOURCES += \
    images.qrc


