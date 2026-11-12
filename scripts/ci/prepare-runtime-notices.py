#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Prepare finite exact-source CI runtime notices; never used at app runtime."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

SOURCES = {('libdrm', '2.4.115'): {'sha256': '554cfbfe0542bddb391b4e3e05bfbbfc3e282b955bd56218d21c0616481f65eb',
                         'url': 'https://dri.freedesktop.org/libdrm/libdrm-2.4.115.tar.xz',
                         'authority': 'https://raw.githubusercontent.com/buildroot/buildroot/2023.02/package/libdrm/libdrm.hash'},
 ('libdrm', '2.4.128'): {'sha256': '3bb35db8700c2a0b569f2c6729a53f5495786856b310854c8de57782a22bddac',
                         'url': 'https://dri.freedesktop.org/libdrm/libdrm-2.4.128.tar.xz',
                         'authority': 'https://raw.githubusercontent.com/buildroot/buildroot/e68414dc61b84d0f69c1ed1bb30bcd3d67473eb4/package/libdrm/libdrm.hash'},
 ('libdrm', '2.4.133'): {'sha256': 'fc68f9d0ba2ea63c9432a299e14fea09fad7a8a66e8039fcd7802ca59f77b4f5',
                         'url': 'https://dri.freedesktop.org/libdrm/libdrm-2.4.133.tar.xz',
                         'authority': 'https://www.mail-archive.com/dri-devel%40lists.freedesktop.org/msg603051.html'},
 ('libdrm', '2.4.134'): {'sha256': 'ac5e74d157830eb8bee44c6a6bf3ad49774ef0dd2a72bdad74a8f20308b52a95',
                         'url': 'https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz',
                         'authority': 'https://raw.githubusercontent.com/buildroot/buildroot/a2602d10a16e1d32d619fa8dfb696df81265a2c9/package/libdrm/libdrm.hash',
                         'mirror': 'https://sources.buildroot.net/libdrm/libdrm-2.4.134.tar.xz'},
 ('e2fsprogs', '1.47.4'): {'sha256': 'fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b',
                           'url': 'https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.4/e2fsprogs-1.47.4.tar.xz',
                           'authority': 'https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.4/sha256sums.asc'},
 ('xcb-util-keysyms', '0.4.1'): {'sha256': '7c260a5294412aed429df1da2f8afd3bd07b7cba3fec772fba15a613a6d5c638',
                                 'url': 'https://www.x.org/releases/individual/xcb/xcb-util-keysyms-0.4.1.tar.xz',
                                 'authority': 'https://lists.x.org/archives/xorg-announce/2022-October/003233.html',
                                 'mirror': 'https://sources.buildroot.net/xcb-util-keysyms/xcb-util-keysyms-0.4.1.tar.xz'},
 ('glib', '2.90.1'): {'sha256': '93c941aa17d5eb1d53fe838365f29a8b4e539c222a256d974ec8f30fc413e396',
                      'url': 'https://download.gnome.org/sources/glib/2.90/glib-2.90.1.tar.xz',
                      'authority': 'https://download.gnome.org/sources/glib/2.90/glib-2.90.1.sha256sum'},
 ('libX11', '1.8.14'): {'sha256': 'bbc78c46217174655c2b32b138ad119f3ac15f50808b4d0b3a2677a4b1afc950',
                        'url': 'https://xorg.freedesktop.org/archive/individual/lib/libX11-1.8.14.tar.xz',
                        'authority': 'https://www.mail-archive.com/xorg%40lists.x.org/msg08359.html'},
 ('libgudev', '238'): {'sha256': '61266ab1afc9d73dbc60a8b2af73e99d2fdff47d99544d085760e4fa667b5dd1',
                       'url': 'https://download.gnome.org/sources/libgudev/238/libgudev-238.tar.xz',
                       'authority': 'https://download.gnome.org/sources/libgudev/238/libgudev-238.sha256sum'},
 ('libXext', '1.3.7'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libXext-1.3.7.tar.xz',
                        'sha256': '6c643c7035cdacf67afd68f25d01b90ef889d546c9fcd7c0adf7c2cf91e3a32d',
                        'authority': 'X https://lists.x.org/archives/xorg-announce/2026-January/003660.html'},
 ('libXi', '1.8.4'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libXi-1.8.4.tar.xz',
                      'sha256': 'ddd6057b89a91903bc2f884d55c8467440906945e482c36547260dff28661afc',
                      'authority': 'X https://lists.x.org/archives/xorg-announce/2026-October/003754.html'},
 ('libXinerama', '1.1.6'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libXinerama-1.1.6.tar.xz',
                            'sha256': 'd00fc1599c303dc5cbc122b8068bdc7405d6fcb19060f4597fc51bd3a8be51d7',
                            'authority': 'X https://lists.x.org/archives/xorg-announce/2026-January/003661.html'},
 ('libXrandr', '1.5.5'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libXrandr-1.5.5.tar.xz',
                          'sha256': '72b922c2e765434e9e9f0960148070bd4504b288263e2868a4ccce1b7cf2767a',
                          'authority': 'X https://lists.x.org/archives/xorg-announce/2026-January/003664.html'},
 ('libXrender', '0.9.13'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libXrender-0.9.13.tar.xz',
                            'sha256': '3fe88c609df6782e1e7af2e4de36b2563d70cd70ff7dcb1cda561ad64912bb64',
                            'authority': 'X https://lists.x.org/archives/xorg-announce/2026-October/003756.html'},
 ('libXtst', '1.2.6'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libXtst-1.2.6.tar.xz',
                        'sha256': 'f94f0b37c07d80d877f3eda2c2405cc7e62aa9842f6310354aa349e3d66a4fe5',
                        'authority': 'X https://lists.x.org/archives/xorg-announce/2026-October/003757.html'},
 ('libxcb', '1.17.0'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/libxcb-1.17.0.tar.xz',
                        'sha256': '599ebf9996710fea71622e6e184f3a8ad5b43d0e5fa8c4e407123c88a59a6d55',
                        'authority': 'X https://lists.x.org/archives/xorg-announce/2024-April/003507.html'},
 ('xcb-util', '0.4.1'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/xcb-util-0.4.1.tar.xz',
                         'sha256': '5abe3bbbd8e54f0fa3ec945291b7e8fa8cfd3cccc43718f8758430f94126e512',
                         'authority': 'X https://lists.x.org/archives/xorg-announce/2022-December/003306.html'},
 ('xcb-util-cursor', '0.1.6'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/xcb-util-cursor-0.1.6.tar.xz',
                                'sha256': 'fdeb8bd127873519be5cc70dcd0d3b5d33b667877200f9925a59fdcad8f7a933',
                                'authority': 'X '
                                             'https://lists.x.org/archives/xorg-announce/2025-September/003625.html'},
 ('xcb-util-image', '0.4.1'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/xcb-util-image-0.4.1.tar.xz',
                               'sha256': 'ccad8ee5dadb1271fd4727ad14d9bd77a64e505608766c4e98267d9aede40d3d',
                               'authority': 'X '
                                            'https://lists.x.org/archives/xorg-announce/2022-October/003231.html'},
 ('xcb-util-keysyms', '0.4.0'): {'url': 'https://xcb.freedesktop.org/dist/xcb-util-keysyms-0.4.0.tar.bz2',
                                 'sha256': '0ef8490ff1dede52b7de533158547f8b454b241aa3e4dcca369507f66f216dd9',
                                 'authority': 'X '
                                              'https://lists.x.org/archives/xorg-announce/2014-October/002485.html'},
 ('xcb-util-renderutil', '0.3.10'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/xcb-util-renderutil-0.3.10.tar.xz',
                                     'sha256': '3e15d4f0e22d8ddbfbb9f5d77db43eacd7a304029bf25a6166cc63caa96d04ba',
                                     'authority': 'X '
                                                  'https://lists.x.org/archives/xorg-announce/2022-October/003234.html'},
 ('xcb-util-wm', '0.4.2'): {'url': 'https://xorg.freedesktop.org/archive/individual/lib/xcb-util-wm-0.4.2.tar.xz',
                            'sha256': '62c34e21d06264687faea7edbf63632c9f04d55e72114aa4a57bb95e4f888a0b',
                            'authority': 'X https://lists.x.org/archives/xorg-announce/2022-October/003235.html'},
 ('brotli', '1.2.1'): {'url': 'https://github.com/google/brotli/archive/refs/tags/v1.2.1.tar.gz',
                       'sha256': '98d35d11a78dc453aebb7d10c3748f3f73ff78c8267c0dc697b8631f67bf7e8f',
                       'authority': 'D '
                                    'https://raw.githubusercontent.com/freebsd/freebsd-ports/1f82b2df36ccb735b728eb37d2c23f142fc8e9fe/archivers/brotli/distinfo '
                                    '(GH0 filename equivalence pending actual bytes)'},
 ('graphite2', '1.3.15'): {'url': 'https://github.com/silnrsi/graphite/releases/download/1.3.15/graphite2-1.3.15.tgz',
                           'sha256': 'c6bc8b4252724665297f7cad0c55897285c673f9b8e6db3522ace833593fe0b1',
                           'authority': 'G '
                                        'https://github.com/silnrsi/graphite/releases/download/1.3.15/graphite2-1.3.15.sha256sum '
                                        '(full source, not minimal substitute)'},
 ('icu', '78.3'): {'url': 'https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-sources.tgz',
                   'sha256': '3a2e7a47604ba702f345878308e6fefeca612ee895cf4a5f222e7955fabfe0c0',
                   'authority': 'G https://api.github.com/repos/unicode-org/icu/releases/tags/release-78.3 '
                                '(published asset digest; compressed 27977255 bytes)'},
 ('krb5', '1.22.2'): {'url': 'https://kerberos.org/dist/krb5/1.22/krb5-1.22.2.tar.gz',
                      'sha256': '3243ffbc8ea4d4ac22ddc7dd2a1dc54c57874c40648b60ff97009763554eaf13',
                      'authority': 'B '
                                   'https://raw.githubusercontent.com/buildroot/buildroot/261a30b253068338d8c4f1548828eddef754ffe0/package/libkrb5/libkrb5.hash'},
 ('libidn2', '2.3.8'): {'url': 'https://ftp.gnu.org/gnu/libidn/libidn2-2.3.8.tar.gz',
                        'sha256': 'f557911bf6171621e1f72ff35f5b1825bb35b52ed45325dcdee931e5d3c0787a',
                        'authority': 'B '
                                     'https://raw.githubusercontent.com/buildroot/buildroot/261a30b253068338d8c4f1548828eddef754ffe0/package/libidn2/libidn2.hash'},
 ('libportal', '0.11.0'): {'url': 'https://github.com/flatpak/libportal/releases/download/0.11.0/libportal-0.11.0.tar.xz',
                           'sha256': 'abb9e452a1c372133bd3eb8f0bb30d07b2d0a8fb02001c4155cf287555db4040',
                           'authority': 'G '
                                        'https://github.com/flatpak/libportal/releases/download/0.11.0/libportal-0.11.0.tar.xz.sha256sum'},
 ('libunistring', '1.4.2'): {'url': 'https://ftp.gnu.org/gnu/libunistring/libunistring-1.4.2.tar.xz',
                             'sha256': '5b46e74377ed7409c5b75e7a96f95377b095623b689d8522620927964a41499c',
                             'authority': 'B '
                                          'https://raw.githubusercontent.com/buildroot/buildroot/2026.08/package/libunistring/libunistring.hash'},
 ('mtdev', '1.1.6'): {'url': 'https://bitmath.org/code/mtdev/mtdev-1.1.6.tar.bz2',
                      'sha256': '15d7b28da8ac71d8bc8c9287c2045fd174267bc740bec10cfda332dc1204e0e0',
                      'authority': 'B '
                                   'https://raw.githubusercontent.com/buildroot/buildroot/2024.02/package/mtdev/mtdev.hash'},
 ('systemd', '262.1'): {'url': 'https://github.com/systemd/systemd/archive/v262.1/systemd-262.1.tar.gz',
                        'sha256': '100fc873b848b9b0297d8a705e8e2ed326256c8483d9960ee1ffe264c8a5c3e3',
                        'authority': 'B '
                                     'https://raw.githubusercontent.com/buildroot/buildroot/913e48076e9e6ee9fa106bfe812ceb8e54a70339/package/systemd/systemd.hash '
                                     '(same commit systemd.mk uses github macro/v262.1)'},
 ('systemd', '262.2'): {'url': 'https://github.com/systemd/systemd/archive/v262.2/systemd-262.2.tar.gz',
                        'sha256': '116432ee9dce9bfef4c7da7963c6de41899ab44197f2876ee99545707a5c7c1f',
                        'authority': 'G https://github.com/systemd/systemd/releases/tag/v262.2 '
                                     '(SHA256 computed locally from the fixed tag bytes; no independently published hash)',
                        'authority_kind': 'official fixed source identity; SHA256 computed locally, not '
                                          'independently published or signature verified'},
 ('tslib', '1.24'): {'url': 'https://github.com/libts/tslib/releases/download/1.24/tslib-1.24.tar.xz',
                     'sha256': '58d9941ffaa269c399f00d0d308184c96087f7acf69aa8e3c6645e852f993ba2',
                     'authority': 'G '
                                  'https://github.com/libts/tslib/releases/download/1.24/tslib-1.24.tar.xz.sha256; '
                                  'B '
                                  'https://raw.githubusercontent.com/buildroot/buildroot/261a30b253068338d8c4f1548828eddef754ffe0/package/tslib/tslib.hash'},
 ('libproxy', '0.5.12'): {'url': 'https://github.com/libproxy/libproxy/archive/refs/tags/0.5.12.tar.gz',
                          'sha256': 'a1fa55991998b80a567450a9e84382421a7176a84446c95caaa8b72cf09fa86f',
                          'authority': 'Official annotated tag object `99e8700a7ffff872e80d27adcc2d163ac8fef7a7` '
                                       '-> commit `99da01926b1b1e303a4d2331bbd74bed424863e7`; SHA256 computed '
                                       'locally. Debian DSC hash is for a different compressed envelope; all 106 '
                                       'ordinary file contents/mode/mtime agree exactly, see batch2 receipt. No '
                                       'signature claim.',
                          'authority_kind': 'official fixed source identity; SHA256 computed locally, not '
                                            'independently published or signature verified',
                          'source_commit': '99da01926b1b1e303a4d2331bbd74bed424863e7'},
 ('keyutils', '1.6.3'): {'url': 'https://git.kernel.org/pub/scm/linux/kernel/git/dhowells/keyutils.git/snapshot/keyutils-1.6.3.tar.gz',
                         'sha256': 'a61d5706136ae4c05bd48f86186bcfdbd88dd8bd5107e3e195c924cfc1b39bb4',
                         'authority': 'B '
                                      'https://raw.githubusercontent.com/buildroot/buildroot/261a30b253068338d8c4f1548828eddef754ffe0/package/keyutils/keyutils.hash '
                                      '(source URL from same commit keyutils.mk)',
                         'mirror': 'https://sources.buildroot.net/keyutils/keyutils-1.6.3.tar.gz'},
 ('gcc', '16.2.1+git9901'): {'url': 'https://api.opensuse.org/public/source/openSUSE:Factory/gcc16/gcc-16.2.1+git9901.tar.xz?rev=13',
                             'sha256': '0dff62fde25a78f75347e9251326c03db35d2572d6692fd6ae3c9f60730736f9',
                             'authority': 'Official revision13/srcmd5 `975fdbd383304dfa6798fd8a54ce331a`, '
                                          'upstream gcc.obsinfo commit '
                                          '`fa86c5b409fa1c265ddb1cb564b32961bbe943001ada57386275e19372e50bdf`; '
                                          'SHA256 computed locally from fixed official bytes; no independently '
                                          'published SHA256/signature claim.',
                             'authority_kind': 'official fixed source identity; SHA256 computed locally, not '
                                               'independently published or signature verified',
                             'source_revision': '13',
                             'source_md5': '975fdbd383304dfa6798fd8a54ce331a',
                             'source_commit': 'fa86c5b409fa1c265ddb1cb564b32961bbe943001ada57386275e19372e50bdf'},
 ('gcc', '16.2.1+git9713'): {'url': 'https://api.opensuse.org/public/source/openSUSE:Factory/gcc16/gcc-16.2.1+git9713.tar.xz?rev=12',
                             'sha256': '799ed061638a96a66318ff43bad5a85487d7dc371578cca1de9b5d2fbee62410',
                             'authority': 'Official revision12/srcmd5 `3375872901fc83888c8145116e865305`, '
                                          'upstream gcc.obsinfo commit '
                                          '`4fcaf5b1154d9cb929c5e47b509f59a801e94a5c`; SHA256 computed locally '
                                          'from fixed official bytes; no independently published SHA256/signature '
                                          'claim.',
                             'authority_kind': 'official fixed source identity; SHA256 computed locally, not '
                                               'independently published or signature verified',
                             'source_revision': '12',
                             'source_md5': '3375872901fc83888c8145116e865305',
                             'source_commit': '4fcaf5b1154d9cb929c5e47b509f59a801e94a5c'}}

SOURCES[('libportal', '0.9.1')] = {
    'url': 'https://github.com/flatpak/libportal/releases/download/0.9.1/libportal-0.9.1.tar.xz',
    'sha256': 'de801ee349ed3c255a9af3c01b1a401fab5b3fc1c35eb2fd7dfb35d4b8194d7f',
    'authority': 'https://api.github.com/repos/flatpak/libportal/releases/tags/0.9.1',
    'authority_kind': 'official fixed release asset; SHA256 computed locally, not independently published or signature verified',
    'source_commit': '8f5dc8d192f6e31dafe69e35219e3b707bde71ce',
}

# The official kernel snapshot returns 403; exact reviewed mirror bytes use the same pin.
SOURCES[('keyutils', '1.6.3')]['upstream_url'] = SOURCES[('keyutils', '1.6.3')]['url']
SOURCES[('keyutils', '1.6.3')]['url'] = SOURCES[('keyutils', '1.6.3')].pop('mirror')

SONAMES = {'brotli': {'libbrotlidec.so.1', 'libbrotlicommon.so.1'},
 'e2fsprogs': {'libcom_err.so.2'},
 'gcc': {'libgomp.so.1', 'libgcc_s.so.1', 'libstdc++.so.6'},
 'glib': {'libgio-2.0.so.0',
          'libgirepository-2.0.so.0',
          'libglib-2.0.so.0',
          'libgmodule-2.0.so.0',
          'libgobject-2.0.so.0',
          'libgthread-2.0.so.0'},
 'graphite2': {'libgraphite2.so.3'},
 'icu': {'libicudata.so.78', 'libicui18n.so.78', 'libicuuc.so.78'},
 'keyutils': {'libkeyutils.so.1'},
 'krb5': {'libkrb5.so.3', 'libkrb5support.so.0', 'libgssapi_krb5.so.2', 'libk5crypto.so.3'},
 'libX11': {'libX11.so.6', 'libX11-xcb.so.1'},
 'libXext': {'libXext.so.6'},
 'libXi': {'libXi.so.6'},
 'libXinerama': {'libXinerama.so.1'},
 'libXrandr': {'libXrandr.so.2'},
 'libXrender': {'libXrender.so.1'},
 'libXtst': {'libXtst.so.6'},
 'libdrm': {'libdrm.so.2',
            'libdrm_amdgpu.so.1',
            'libdrm_etnaviv.so.1',
            'libdrm_exynos.so.1',
            'libdrm_freedreno.so.1',
            'libdrm_intel.so.1',
            'libdrm_nouveau.so.2',
            'libdrm_omap.so.1',
            'libdrm_radeon.so.1',
            'libdrm_tegra.so.0'},
 'libgudev': {'libgudev-1.0.so.0'},
 'libidn2': {'libidn2.so.0'},
 'libportal': {'libportal.so.1'},
 'libproxy': {'libproxy.so.1', 'libpxbackend-1.0.so'},
 'libunistring': {'libunistring.so.5'},
 'libxcb': {'libxcb-glx.so.0',
            'libxcb-randr.so.0',
            'libxcb-render.so.0',
            'libxcb-shape.so.0',
            'libxcb-shm.so.0',
            'libxcb-sync.so.1',
            'libxcb-xfixes.so.0',
            'libxcb-xinput.so.0',
            'libxcb-xkb.so.1',
            'libxcb.so.1'},
 'mtdev': {'libmtdev.so.1'},
 'systemd': {'libsystemd.so.0', 'libudev.so.1'},
 'tslib': {'libts.so.0'},
 'xcb-util': {'libxcb-util.so.1'},
 'xcb-util-cursor': {'libxcb-cursor.so.0'},
 'xcb-util-image': {'libxcb-image.so.0'},
 'xcb-util-keysyms': {'libxcb-keysyms.so.1'},
 'xcb-util-renderutil': {'libxcb-render-util.so.0'},
 'xcb-util-wm': {'libxcb-icccm.so.4'}}

# Profiles contain complete source roots, not sampled test-oracle selections.
PROFILES = {
    'libdrm': {'legal': [], 'roots': [''], 'foundations': ['xf86drm.c'],
               'legacy_comments': True, 'include_all_sources': True, 'marker': b'permission'},
    'e2fsprogs': {'legal': ['NOTICE'], 'roots': ['lib/et/'], 'foundations': ['lib/et/com_err.c'],
                 'legacy_comments': True, 'include_all_sources': True, 'marker': b'permission'},
    'xcb-util-keysyms': {'legal': ['COPYING'], 'roots': ['keysyms/'], 'foundations': ['keysyms/keysyms.c']},
    'glib': {'legal': ['COPYING'], 'license_directory': True,
             'roots': ['glib/', 'gobject/', 'gmodule/', 'gthread/', 'gio/', 'girepository/'],
             'foundations': ['glib/glib.h', 'gobject/gobject.h', 'gmodule/gmodule.h', 'gthread/gthread-impl.c',
                             'gio/gio.h', 'girepository/girepository.h'], 'marker': b'spdx-license-identifier: lgpl-2.1'},
    'libX11': {'legal': ['COPYING'], 'roots': ['src/', 'include/'],
               'foundations': ['src/XlibInt.c', 'src/x11_xcb.c']},
    'libgudev': {'legal': ['COPYING'], 'roots': ['gudev/'],
                 'foundations': ['gudev/gudevclient.c', 'gudev/gudev.h'], 'marker': b'spdx-license-identifier: lgpl-2.1'},
    'libXext': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/extutil.c']},
    'libXi': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/XIQueryVersion.c']},
    'libXinerama': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/Xinerama.c']},
    'libXrandr': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/Xrandr.c']},
    'libXrender': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/Xrender.c']},
    'libXtst': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/XTest.c', 'src/XRecord.c']},
    'xcb-util': {'legal': ['COPYING'], 'roots': ['src/'], 'foundations': ['src/xcb_aux.c']},
    'xcb-util-cursor': {'legal': ['COPYING'], 'roots': ['cursor/'], 'foundations': ['cursor/cursor.c']},
    'xcb-util-image': {'legal': ['COPYING'], 'roots': ['image/'], 'foundations': ['image/xcb_image.c']},
    'xcb-util-wm': {'legal': ['COPYING'], 'roots': ['icccm/'], 'foundations': ['icccm/icccm.c']},
    'xcb-util-renderutil': {'legal': ['COPYING'], 'roots': ['renderutil/'], 'foundations': ['renderutil/glyph.c']},
    'libxcb': {'legal': ['COPYING'], 'roots': ['src/'], 'foundations': ['src/xcb_conn.c'], 'protocols': True},
    'brotli': {'legal': ['LICENSE'], 'roots': ['c/common/', 'c/dec/', 'c/include/brotli/'],
               'foundations': ['c/common/constants.c', 'c/dec/decode.c']},
    'graphite2': {'legal': ['COPYING', 'LICENSE'], 'roots': ['src/', 'include/graphite2/'],
                  'foundations': ['src/Face.cpp'], 'cpp': True},
    'krb5': {'legal': ['NOTICE', 'src/lib/gssapi/LICENSE', 'src/include/copyright.h'],
             'roots': ['src/lib/', 'src/include/'], 'foundations': ['src/include/copyright.h'], 'assembly': True},
    'icu': {'root': 'icu', 'legal': ['LICENSE', 'license.html'],
            'roots': ['source/common/', 'source/i18n/', 'source/data/'],
            'foundations': ['source/common/unicode/utypes.h'], 'cpp': True},
    'mtdev': {'legal': ['COPYING'], 'roots': ['src/', 'include/'], 'foundations': ['src/common.h']},
    'libproxy': {'legal': ['COPYING'], 'roots': ['src/libproxy/', 'src/backend/'],
                 'foundations': ['src/libproxy/proxy.c', 'src/backend/px-manager.c']},
    'tslib': {'legal': ['COPYING'], 'roots': ['src/'], 'foundations': ['src/ts_config.c']},
    'libidn2': {'legal': ['COPYING', 'COPYINGv2', 'COPYING.LESSERv3', 'COPYING.unicode'],
                'roots': ['lib/'], 'foundations': []},
    'keyutils': {'legal': ['LICENCE.GPL', 'LICENCE.LGPL'], 'roots': [''], 'foundations': ['keyutils.c', 'keyutils.h']},
    'libportal': {'legal': ['COPYING'], 'roots': ['libportal/'], 'foundations': ['libportal/account.c'], 'gpl3': True},
    'systemd': {'legal': ['LICENSE.GPL2', 'LICENSE.LGPL2.1'], 'license_directory': True,
                'roots': ['src/libsystemd/', 'src/libudev/', 'src/basic/', 'src/fundamental/', 'src/shared/'],
                'foundations': ['src/libsystemd/sd-event/sd-event.c', 'src/libudev/libudev.c']},
    'libunistring': {'legal': ['COPYING', 'COPYING.LIB'], 'roots': ['lib/'], 'foundations': []},
    'gcc': {'legal': ['COPYING', 'COPYING.LIB', 'COPYING3', 'COPYING3.LIB', 'COPYING.RUNTIME',
                      'include/COPYING', 'include/COPYING3', 'libiberty/COPYING.LIB',
                      'libstdc++-v3/include/pstl/LICENSE.txt'],
            'roots': ['libgcc/', 'libgomp/', 'libstdc++-v3/include/', 'libstdc++-v3/src/',
                      'libstdc++-v3/config/', 'libstdc++-v3/libsupc++/', 'include/', 'libiberty/'],
            'foundations': ['libgomp/parallel.c', 'libgcc/config/aarch64/lse.S'], 'cpp': True,
            'assembly': True, 'stream_comments': True},
}
SECONDARY = {
    'gpl3': {'url': 'https://www.gnu.org/licenses/gpl-3.0.txt',
             'sha256': '3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986'},
    'xcb-proto': {'url': 'https://xorg.freedesktop.org/archive/individual/proto/xcb-proto-1.17.0.tar.xz',
                  'sha256': '2c1bacd2110f4799f74de6ebb714b94cf6f80fb112316b1219480fd22562148c',
                  'authority': 'https://lists.x.org/archives/xorg-announce/2024-April/003506.html'},
}


def profile(provider, version):
    result = dict(PROFILES[provider])
    result['root'] = result.get('root', provider + '-' + version)
    if (provider, version) == ('xcb-util-keysyms', '0.4.0'):
        result['legal'] = []  # The exact original archive has only authored keysyms.c grants.
    result['gcc'] = provider == 'gcc'
    return result


def profile_for_root(root):
    if root == 'xcb-proto-1.17.0':
        return {'root': root, 'legal': ['COPYING'], 'roots': [], 'foundations': [], 'xml': True}
    for provider, version in SOURCES:
        candidate = profile(provider, version)
        if candidate['root'] == root:
            return candidate
    raise RuntimeError('Unknown exact source archive root')


def source_comments(data):
    # C++ raw literals are opaque, including quotes and comment bait inside them.
    tokens = re.finditer(
        rb'(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\(.*?\)(?P=delimiter)"'
        rb'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
        rb'|(?P<comment>/\*.*?\*/(?:\r?\n)?|//[^\n]*(?:\n|$)(?:[ \t]*//[^\n]*(?:\n|$))*)', data, re.S)
    return [token.group('comment') for token in tokens if token.group('comment') is not None]


def assembly_comments(data):
    # Outer assembler comments take precedence over C-looking examples inside them.
    blocks, current, c_plane = [], [], []
    def finish():
        if current and any(b'copyright' in line.lower() for line in current):
            blocks.append(b''.join(current))
        del current[:]
    in_c = False
    for line in data.splitlines(keepends=True):
        if in_c:
            c_plane.append(line)
            in_c = b'*/' not in line
        elif re.match(rb'^\s*#\s*(?:include|define|if|elif|else|endif|undef|error|pragma)\b', line):
            finish()
            c_plane.append(line)
        elif re.match(rb'^\s*(?:#+|;+|!+)', line) or current and not line.strip():
            current.append(line)
        else:
            finish()
            c_plane.append(line)
            if b'/*' in line:
                in_c = b'*/' not in line
    finish()
    return source_comments(b''.join(c_plane)) + blocks


GCC_SOURCE_TEMPLATES = {
    'libgcc/libgcc-std.ver.in', 'libgomp/omp_lib.f90.in',
    'libgomp/libgomp_f.h.in', 'libgomp/omp.h.in', 'libgomp/omp_lib.h.in',
    'libstdc++-v3/src/c++23/std.cc.in', 'libstdc++-v3/src/c++23/std.compat.cc.in',
    'libstdc++-v3/src/libbacktrace/backtrace-supported.h.in',
}
GCC_OUTER_COMMENT_TEMPLATES = {
    'libgcc/libgcc-std.ver.in', 'libgomp/omp_lib.f90.in', 'libgomp/omp_lib.h.in',
}


def source_file_selected(relative, settings):
    if not settings.get('include_all_sources') and any(part in {'tests', 'test', 'testsuite', 'doc', 'docs'} for part in PurePosixPath(relative).parts):
        return False
    if settings.get('gcc') and (relative in GCC_SOURCE_TEMPLATES or
                                relative.startswith('libstdc++-v3/include/') and
                                '.' not in PurePosixPath(relative).name):
        return True
    suffixes = ('.c', '.h')
    if settings.get('cpp'):
        suffixes += ('.cc', '.cpp', '.hh', '.hpp', '.inc', '.cxx', '.hxx', '.tcc', '.def')
    if settings.get('assembly'):
        suffixes += ('.S', '.s', '.asm')
    return relative.endswith(suffixes) and any(relative.startswith(prefix) for prefix in settings['roots'])


def source_files(data, expected_root):
    settings = profile_for_root(expected_root)
    gcc = settings.get('gcc', False)
    max_members = 180000 if gcc else 20000
    max_expanded = 1024 * 1024 * 1024 if gcc else 256 * 1024 * 1024
    files, seen = {}, set()
    expanded = selected = count = 0
    copying_alias = None
    started = time.monotonic()
    with tarfile.open(fileobj=io.BytesIO(data), mode='r|*') as archive:
        for member in archive:
            count += 1
            expanded += member.size
            if count > max_members or member.size < 0 or expanded > max_expanded:
                raise RuntimeError('Source archive member/expanded size limit exceeded')
            if time.monotonic() - started > 180:
                raise RuntimeError('Source archive processing timeout limit exceeded')
            path = PurePosixPath(member.name)
            if (path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != expected_root
                    or str(path) in seen or any(ord(char) < 32 or ord(char) == 127 for char in member.name)):
                raise RuntimeError('Unsafe, duplicate or wrong-version source member')
            seen.add(str(path))
            relative = '/'.join(path.parts[1:])
            if member.issym() or member.islnk():
                target = (member.linkname if member.islnk() else
                          posixpath.join(posixpath.dirname(member.name), member.linkname))
                normalized = PurePosixPath(posixpath.normpath(target))
                if (normalized.is_absolute() or not normalized.parts or normalized.parts[0] != expected_root
                        or any(ord(char) < 32 or ord(char) == 127 for char in member.linkname)):
                    raise RuntimeError('Escaping source archive link')
                if expected_root == 'glib-2.90.1' and relative == 'COPYING':
                    if not member.issym() or member.linkname != 'LICENSES/LGPL-2.1-or-later.txt':
                        raise RuntimeError('Unsupported GLib COPYING alias')
                    copying_alias = expected_root + '/' + member.linkname
                continue
            if member.isdir():
                continue
            if not member.isfile():
                raise RuntimeError('Unsupported source archive member')
            legal = relative in settings['legal'] or settings.get('license_directory') and relative.startswith('LICENSES/')
            source = source_file_selected(relative, settings)
            xml = settings.get('xml') and relative.startswith('src/') and relative.endswith('.xml')
            if not legal and not source and not xml:
                continue
            if gcc and legal and member.size > 2 * 1024 * 1024:
                raise RuntimeError('Selected notice size limit exceeded')
            if gcc and source and member.size > 32 * 1024 * 1024:
                raise RuntimeError('Runtime source member size bound exceeded')
            payload = archive.extractfile(member).read()
            if gcc and source and not legal:
                comments = (assembly_comments(payload)
                            if relative.endswith(('.S', '.s', '.asm')) or relative in GCC_OUTER_COMMENT_TEMPLATES
                            else source_comments(payload))
                if any(len(block) > 2 * 1024 * 1024 for block in comments):
                    raise RuntimeError('Selected notice size limit exceeded')
                payload = b'\n'.join(comments)
            if gcc:
                selected += len(payload)
                # Keep at most one selected-notice chunk of assembly headroom.
                # The actual serialized output limit is still exactly 32 MiB below.
                if selected > (32 + 2) * 1024 * 1024:
                    raise RuntimeError('Selected notice assembly buffer size limit exceeded')
            files[str(path)] = payload
    if copying_alias is not None:
        if copying_alias not in files:
            raise RuntimeError('Missing ordinary GLib COPYING alias target')
        files[expected_root + '/COPYING'] = files[copying_alias]
    return files


def verified_secondary(name):
    pin = SECONDARY[name]
    data = download_source(pin['url'])
    if hashlib.sha256(data).hexdigest() != pin['sha256']:
        raise RuntimeError('Secondary notice source checksum mismatch: ' + name)
    return data


def protocol_notices():
    root = 'xcb-proto-1.17.0'
    files = source_files(verified_secondary('xcb-proto'), root)
    copying = files.get(root + '/COPYING', b'')
    if not copying.strip():
        raise RuntimeError('Missing complete xcb-proto COPYING')
    selected = {'xproto', 'bigreq', 'xc_misc', 'randr', 'render', 'shape', 'shm', 'sync',
                'xfixes', 'xinput', 'xkb', 'glx'}
    done, notices = set(), []
    allowed = set(selected)
    while selected - done:
        name = sorted(selected - done)[0]
        if name not in allowed:
            raise RuntimeError('Unknown finite protocol import: ' + name)
        data = files.get(root + '/src/' + name + '.xml')
        if data is None:
            raise RuntimeError('Missing finite protocol XML: ' + name)
        document = ET.fromstring(data)
        for imported in document.findall('import'):
            if imported.text not in allowed:
                raise RuntimeError('Unknown finite protocol import')
            selected.add(imported.text)
        # Raw XML comments preserve BSD grants without a Permission keyword too.
        comments = re.findall(rb'<!--.*?-->', data, re.S)
        notices.append(b'\nSOURCE ' + name.encode('ascii') + b'.xml\n' + b'\n'.join(comments))
        done.add(name)
    return copying, b'\n'.join(notices)


def validate_sonames(provider, names):
    if provider not in SONAMES or not names or not set(names).issubset(SONAMES[provider]):
        raise RuntimeError('Unknown or missing exact runtime SONAME')


def prepare(provider, version, archive_bytes, sonames, destination, package_identity):
    pin = SOURCES.get((provider, version))
    if pin is None:
        raise RuntimeError('Unknown exact runtime source version')
    validate_sonames(provider, sonames)
    if hashlib.sha256(archive_bytes).hexdigest() != pin['sha256']:
        raise RuntimeError('Runtime source archive checksum mismatch')
    if any(ord(char) < 32 or ord(char) == 127 for char in package_identity) or len(package_identity) > 4096:
        raise RuntimeError('Unsafe installed package identity')
    settings = profile(provider, version)
    root = settings['root']
    files = source_files(archive_bytes, root)
    if any(root + '/' + name not in files for name in settings['foundations']):
        raise RuntimeError('Missing foundational runtime source notice')
    legal = {}
    for name in settings['legal']:
        data = files.get(root + '/' + name, b'')
        if not data.strip():
            raise RuntimeError('Missing or empty complete runtime package notice: ' + name)
        legal[name] = data
    if settings.get('license_directory'):
        for name, data in files.items():
            if name.startswith(root + '/LICENSES/'):
                if not data.strip():
                    raise RuntimeError('Missing or empty complete license text')
                legal[name[len(root) + 1:]] = data
        if not any(name.startswith('LICENSES/') for name in legal):
            raise RuntimeError('Missing complete license directory')
    notices = []
    for name, data in sorted(files.items()):
        relative = name[len(root) + 1:]
        if not source_file_selected(relative, settings):
            continue
        comments = ([data] if settings.get('stream_comments') else
                    re.findall(rb'/\*.*?\*/(?:\r?\n)?|(?m:^[ \t]*//[^\n]*(?:\n|$))', data, re.S)
                    if settings.get('legacy_comments') else
                    assembly_comments(data) if relative.endswith(('.S', '.s', '.asm')) else source_comments(data))
        if comments:
            notices.append(b'\nSOURCE ' + name.encode('utf-8') + b'\n' + b'\n'.join(comments))
    content = b'\n'.join(notices)
    lower = content.lower()
    marker = settings.get('marker')
    if (not content or b'copyright' not in lower or marker is not None and marker not in lower
            or marker is None and not any(word in lower for word in
                                          (b'permission', b'redistribution', b'redistribute',
                                           b'spdx-license-identifier', b'general public license'))):
        raise RuntimeError('Missing complete source copyright and permission notice')
    supplements = []
    if settings.get('gpl3'):
        legal['LICENSES/GPL-3.0-only.txt'] = verified_secondary('gpl3')
        supplements.append(dict(SECONDARY['gpl3'], purpose='GPLv3 incorporated by exact LGPLv3 source COPYING'))
    if settings.get('protocols'):
        copying, xml = protocol_notices()
        legal['COPYING.xcb-proto'] = copying
        legal['COPYING.protocol-notices'] = xml
        supplements.append(dict(SECONDARY['xcb-proto'], purpose='exact generated protocol input legal notices'))
    directory = Path(destination).resolve() / (provider + '-' + version)
    if any(char in str(directory) for char in ';=\r\n') or directory.is_symlink():
        raise RuntimeError('Unsafe notice output mapping path')
    provenance = dict(pin, provider=provider, version=version, package_identity=package_identity,
                      sonames=sorted(set(sonames)))
    if supplements:
        provenance['supplements'] = supplements
    serialized = (json.dumps(provenance, indent=2, sort_keys=True) + '\n').encode('utf-8')
    package_notice = legal[settings['legal'][0]] if settings['legal'] else b''
    if settings.get('gcc') and (sum(map(len, legal.values())) + len(content) + len(package_notice) + len(serialized)
                                > 32 * 1024 * 1024):
        raise RuntimeError('Selected notice total output size limit exceeded')
    directory.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.' + provider + '-', dir=str(directory.parent)))
    try:
        (temporary / 'COPYING.source-notices').write_bytes(content)
        if package_notice:
            (temporary / 'COPYING.package-NOTICE').write_bytes(package_notice)
        for name, data in sorted(legal.items()):
            target = temporary / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (temporary / 'provenance.json').write_bytes(serialized)
        if directory.exists():
            marker = directory / 'provenance.json'
            if marker.is_symlink() or not marker.is_file():
                raise RuntimeError('Unowned existing runtime notice output directory')
            previous = json.loads(marker.read_text())
            if any(previous.get(key) != provenance[key] for key in ('provider', 'version', 'sha256')):
                raise RuntimeError('Stale or unowned runtime notice output directory')
            shutil.rmtree(str(directory))
        os.replace(str(temporary), str(directory))
    finally:
        if temporary.exists():
            shutil.rmtree(str(temporary))
    return {name: str(directory) for name in sorted(set(sonames))}


RPM_OWNERS = {
    'libdrm': {'libdrm', 'libdrm2', 'libdrm_amdgpu1', 'libdrm_intel1', 'libdrm_nouveau2', 'libdrm_radeon1',
               'libdrm_freedreno1', 'libdrm_tegra0', 'libdrm_omap1', 'libdrm_exynos1', 'libdrm_etnaviv1'},
    'xcb-util-keysyms': {'xcb-util-keysyms', 'libxcb-keysyms1'},
    'libX11': {'libX11-6', 'libX11-xcb1'},
    'libXext': {'libXext6'}, 'libXi': {'libXi6'}, 'libXinerama': {'libXinerama1'},
    'libXrandr': {'libXrandr2'}, 'libXrender': {'libXrender1'}, 'libXtst': {'libXtst6'},
    'brotli': {'libbrotlicommon1', 'libbrotlidec1'}, 'e2fsprogs': {'libcom_err2'},
    'gcc': {'libgcc_s1', 'libgomp1', 'libstdc++6'}, 'graphite2': {'libgraphite2-3'},
    'krb5': {'krb5'}, 'icu': {'libicu78', 'libicu78-ledata', 'libicu78-bedata'},
    'mtdev': {'libmtdev1'}, 'libproxy': {'libproxy1', 'libpxbackend-1_0'}, 'tslib': {'libts0'},
    'xcb-util-cursor': {'libxcb-cursor0'}, 'xcb-util-wm': {'libxcb-icccm4'},
    'xcb-util-image': {'libxcb-image0'}, 'xcb-util-renderutil': {'libxcb-render-util0'},
    'xcb-util': {'libxcb-util1'},
    'libxcb': {'libxcb1', 'libxcb-randr0', 'libxcb-render0', 'libxcb-shape0', 'libxcb-shm0',
               'libxcb-sync1', 'libxcb-xfixes0', 'libxcb-xinput0', 'libxcb-xkb1', 'libxcb-glx0'},
}
PACMAN_PACKAGES = {'e2fsprogs': 'e2fsprogs', 'glib': 'glib2', 'libgudev': 'libgudev',
                   'libidn2': 'libidn2', 'keyutils': 'keyutils', 'libportal': 'libportal',
                   'libproxy': 'libproxy', 'systemd': 'systemd-libs', 'tslib': 'tslib',
                   'libunistring': 'libunistring'}
RPM_FIELDS = ['NAME', 'VERSION', 'RELEASE', 'ARCH', 'SOURCERPM']
RPM_QUERY = '\t'.join('%{' + name + '}' for name in RPM_FIELDS)


def command(args):
    result = subprocess.run(args, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError('Runtime provider metadata command failed: ' + args[0])
    return result.stdout.strip()


def diagnostic(provider, stage, reason, record=None, soname=None):
    parts = ['provider=' + provider, 'stage=' + stage, 'reason=' + reason]
    if record:
        parts.extend(name + '=' + record[name] for name in RPM_FIELDS if name in record)
    if soname:
        parts.append('SONAME=' + soname)
    return RuntimeError('Runtime notice identity rejected: ' + '; '.join(parts))


def package_record(text, provider, soname=None):
    fields = text.split('\t')
    if len(fields) != len(RPM_FIELDS):
        raise diagnostic(provider, 'metadata', 'missing or ambiguous public package fields', soname=soname)
    result = {}
    for name, value in zip(RPM_FIELDS, fields):
        if (not value or len(value) > 160 or not re.fullmatch(r'[A-Za-z0-9_().+~:-]+', value)
                or value.startswith('-')):
            # Do not reproduce unsafe bytes, including the remainder of an injected line.
            raise diagnostic(provider, 'metadata', 'unsafe public field ' + name, soname=soname)
        result[name] = value
    return result


def source_identity(provider, record, soname):
    if record['NAME'] not in RPM_OWNERS[provider]:
        raise diagnostic(provider, 'owner', 'unknown runtime owner', record, soname)
    source = provider
    if provider == 'gcc':
        source = 'gcc16'
    elif provider == 'libproxy':
        source = 'libproxy-client' if record['NAME'] == 'libproxy1' else 'libproxy-backend'
    expected = source + '-' + record['VERSION'] + '-' + record['RELEASE'] + '.src.rpm'
    if record['SOURCERPM'] != expected:
        raise diagnostic(provider, 'source', 'unexpected exact source identity', record, soname)
    if (provider, record['VERSION']) not in SOURCES:
        raise diagnostic(provider, 'version', 'unknown exact upstream source version', record, soname)
    return record['VERSION'], record['RELEASE']


def runtime_filename(provider, name):
    for soname in SONAMES[provider]:
        stem = soname.split('.so', 1)[0]
        if name == soname or re.fullmatch(re.escape(stem) + r'\.so\.[0-9.]+', name):
            return True
    return False


def elf_soname(path, provider, record):
    with path.open('rb') as source:
        header = source.read(20)
    if len(header) < 20 or header[:4] != b'\x7fELF' or header[4] != 2 or header[5] not in (1, 2):
        raise diagnostic(provider, 'elf64', 'missing supported ELF64 header', record, path.name)
    machine = int.from_bytes(header[18:20], 'little' if header[5] == 1 else 'big')
    if machine not in (62, 183):
        raise diagnostic(provider, 'architecture', 'unsupported 64-bit target ABI', record, path.name)
    if record and record.get('ARCH') not in {'x86_64' if machine == 62 else 'aarch64'}:
        raise diagnostic(provider, 'architecture', 'package and actual ELF target disagree', record, path.name)
    try:
        dynamic = command(['readelf', '-d', str(path)])
    except RuntimeError:
        raise diagnostic(provider, 'readelf', 'runtime SONAME query failed', record, path.name)
    matches = re.findall(r'\(SONAME\)[^\n]*\[([^\]]+)\]', dynamic)
    if len(matches) != 1 or matches[0] not in SONAMES[provider]:
        raise diagnostic(provider, 'soname', 'unknown missing or ambiguous exact SONAME', record, path.name)
    return matches[0]


def discover(provider):
    if provider not in SONAMES:
        raise RuntimeError('Unsupported runtime notice provider')
    def query(args, record=None, soname=None):
        try:
            return command(args)
        except RuntimeError:
            raise diagnostic(provider, 'metadata-query', args[0] + ' query failed', record, soname)
    rpm = provider in RPM_OWNERS and (provider not in PACMAN_PACKAGES or bool(shutil.which('rpm')))
    if provider in {'libdrm', 'xcb-util-keysyms', 'libX11'}:
        rpm = True
    if rpm:
        primary_soname = sorted(SONAMES[provider])[0]
        # Prefer the stable source-family anchor rather than a split optional member.
        primary_soname = {'libdrm': 'libdrm.so.2', 'libX11': 'libX11.so.6', 'libxcb': 'libxcb.so.1',
                          'brotli': 'libbrotlicommon.so.1', 'libproxy': 'libproxy.so.1'}.get(provider, primary_soname)
        capability = primary_soname + '()(64bit)'
        primary = package_record(query(['rpm', '-q', '--whatprovides', '--qf', RPM_QUERY, capability], soname=primary_soname),
                                 provider, primary_soname)
        family = source_identity(provider, primary, primary_soname)
        records = {primary['NAME']: primary}
        for line in query(['rpm', '-qa', '--qf', RPM_QUERY + '\n'], primary, primary_soname).splitlines():
            if line.split('\t', 1)[0] not in RPM_OWNERS[provider]:
                continue
            sibling = package_record(line, provider, primary_soname)
            if source_identity(provider, sibling, primary_soname) != family:
                raise diagnostic(provider, 'siblings', 'conflicting installed runtime source family', sibling, primary_soname)
            records[sibling['NAME']] = sibling
        version = primary['VERSION']
        listing = {}
        for name, record in sorted(records.items()):
            for file in query(['rpm', '-ql', name], record, primary_soname).splitlines():
                if runtime_filename(provider, Path(file).name):
                    if file in listing and listing[file] != record:
                        raise diagnostic(provider, 'owner', 'ambiguous listed file owner', record, Path(file).name)
                    listing[file] = record
        selected, visited = set(), set()
        for file, expected in sorted(listing.items()):
            path = Path(file)
            if not path.is_absolute() or not path.is_file():
                raise diagnostic(provider, 'file', 'missing installed runtime file', expected, path.name)
            actual = package_record(query(['rpm', '-qf', '--qf', RPM_QUERY, file], expected, path.name), provider, path.name)
            if actual != expected:
                raise diagnostic(provider, 'owner', 'actual installed file owner differs', actual, path.name)
            resolved = path.resolve()
            if str(resolved) != file:
                target_owner = package_record(query(['rpm', '-qf', '--qf', RPM_QUERY, str(resolved)], expected, path.name), provider, path.name)
                if target_owner['NAME'] not in records or target_owner != records[target_owner['NAME']]:
                    raise diagnostic(provider, 'owner', 'unverified runtime link target owner', target_owner, path.name)
                expected = target_owner
            if str(resolved) not in visited:
                selected.add(elf_soname(path, provider, expected))
                visited.add(str(resolved))
        identity = ' | '.join(' '.join(record[name] for name in RPM_FIELDS) for record in sorted(records.values(), key=lambda r: r['NAME']))
    else:
        package = PACMAN_PACKAGES.get(provider)
        if package is None:
            raise RuntimeError('Unsupported installed package manager for runtime notice provider')
        identity = query(['pacman', '-Q', package], {'NAME': package})
        fields = identity.split()
        if len(fields) != 2 or fields[0] != package or not re.fullmatch(r'[A-Za-z0-9_.+~:-]+', fields[1]):
            raise diagnostic(provider, 'metadata', 'unsafe or unknown installed pacman identity')
        version = fields[1].rsplit('-', 1)[0].split(':')[-1]
        if (provider, version) not in SOURCES:
            raise RuntimeError('Runtime notice identity rejected: provider=' + provider + '; NAME=' + package +
                               '; VERSION=' + fields[1] + '; reason=unknown exact source version')
        selected, visited = set(), set()
        for file in query(['pacman', '-Qlq', package], {'NAME': package, 'VERSION': fields[1]}).splitlines():
            path = Path(file)
            if not runtime_filename(provider, path.name):
                continue
            if not path.is_absolute() or not path.is_file():
                raise RuntimeError('Missing installed runtime provider file: provider=' + provider)
            for owned in {str(path), str(path.resolve())}:
                if query(['pacman', '-Qoq', owned], {'NAME': package, 'VERSION': fields[1]}, path.name) != package:
                    raise RuntimeError('Installed runtime file has a different package owner: provider=' + provider)
            if str(path.resolve()) not in visited:
                selected.add(elf_soname(path, provider, None))
                visited.add(str(path.resolve()))
    validate_sonames(provider, selected)
    return version, identity, sorted(selected)


def download_source(url, limit=32 * 1024 * 1024):
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=60) as response:
                if limit == 32 * 1024 * 1024:
                    data = response.read(limit + 1)
                    if len(data) > limit:
                        raise RuntimeError('Runtime source archive exceeds download size limit')
                    return data
                chunks, total = [], 0
                while True:
                    block = response.read(min(1024 * 1024, limit + 1 - total))
                    if not block:
                        return b''.join(chunks)
                    total += len(block)
                    if total > limit:
                        raise RuntimeError('Runtime source archive exceeds download size limit')
                    chunks.append(block)
        except (urllib.error.URLError, ConnectionResetError, TimeoutError, socket.timeout) as error:
            reason = error.reason if isinstance(error, urllib.error.URLError) else error
            if not isinstance(reason, (ConnectionResetError, TimeoutError, socket.timeout)) or attempt == 2:
                raise
            print('Transient runtime source transport failure; retrying ({}/3)'.format(attempt + 2), file=sys.stderr)
            time.sleep(attempt + 1)


def download_runtime_source(provider, version):
    pin = SOURCES[(provider, version)]
    print('Fetching runtime source provider={} version={} url={}'.format(provider, version, pin['url']), file=sys.stderr)
    def fetch(url):
        if provider == 'gcc':
            return download_source(url, 128 * 1024 * 1024)
        return download_source(url)
    try:
        return fetch(pin['url'])
    except (urllib.error.URLError, ConnectionResetError, TimeoutError, socket.timeout) as error:
        reason = error.reason if isinstance(error, urllib.error.URLError) else error
        if 'mirror' not in pin or not isinstance(reason, (ConnectionResetError, TimeoutError, socket.timeout)):
            raise
    print('Fetching runtime source mirror provider={} version={} url={}'.format(provider, version, pin['mirror']), file=sys.stderr)
    return fetch(pin['mirror'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider', required=True, choices=sorted(SONAMES))
    parser.add_argument('--destination', required=True)
    args = parser.parse_args()
    version, identity, names = discover(args.provider)
    data = download_runtime_source(args.provider, version)
    mapping = prepare(args.provider, version, data, names, args.destination, identity)
    print(';'.join(name + '=' + directory for name, directory in sorted(mapping.items())))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, tarfile.TarError, ET.ParseError) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        sys.exit(1)
