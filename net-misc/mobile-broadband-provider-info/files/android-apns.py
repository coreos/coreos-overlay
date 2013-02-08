#!/usr/bin/env python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from xml.dom.minidom import parse
from collections import defaultdict
import codecs
import sys

def groupby(func, items):
    result = defaultdict(list)
    for x in items:
        result[func(x)].append(x)
    return result

class APNDB:
    """Represents a set of APNs.

    Allows adding and some primitive kinds of searching. Subclasses are expected
    to override add() to enforce their own constraints as necessary. Also
    supports loading contents from a data file.

    """
    ANDROID = 0
    MBPI = 1
    def __init__(self):
        self._apns = []

    def add(self, apn):
        if type(apn) == list:
            self._apns += apn
        else:
            self._apns.append(apn)

    def apns(self):
        return self._apns

    def carriers(self):
        return set([x['nwid'] for x in self._apns])

    def find(self, name, pred):
        return [x for x in self._apns if name in x and pred(x[name])]

    def groupby(self, func):
        return groupby(func, self._apns)

    def filter(self, func):
        return [x for x in self._apns if func(x)]

    def parse(self):
        pass

class AndroidAPNDB(APNDB):
    """Android APN database.

    Supports loading its contents from an android-format XML file.

    """

    def __init__(self):
        APNDB.__init__(self)

    def _parse_android_apn(self, element):
        # The android APN database isn't as rich as the public one; in
        # particular, it doesn't know whether APNs are CDMA or GSM. Assume they
        # are all GSM :\
        apn = {'technology': 'gsm'}
        for k,v in element.attributes.items():
            apn[k] = v
        apn['nwid'] = '%s:%s' % (apn['mcc'], apn['mnc'])
        apn['source'] = APNDB.ANDROID

        if apn['type'] == 'mms': # XXX: ignore MMS APNs
            return None
        return apn

    def parse(self, xmlfile):
        dom = parse(xmlfile)
        apn_lists = dom.getElementsByTagName('apns')
        for apn_list in apn_lists:
            dom_apns = apn_list.getElementsByTagName('apn')
            for dom_apn in dom_apns:
                r = self._parse_android_apn(dom_apn)
                if r:
                    self._apns.append(r)

class MBPIDB(APNDB):
    """Mobile broadband provider info APN database.

    Supports loading its contents from an MBPI-format XML file.

    """

    def __init__(self):
        APNDB.__init__(self)

    def _add_child_key(self, apn, element, name, rename=None):
        if not rename:
            rename = name
        children = element.getElementsByTagName(name)
        # If we have multiple keys, we construct a list...
        values = [x.data for x in children if 'data' in dir(x)]
        if len(values) > 1:
            apn[rename] = values
        elif len(values) == 1:
            apn[rename] = values[0]

    def _parse_mbpi_apn(self, apn, element):
        apn['apn'] = element.getAttribute('value')
        self._add_child_key(apn, element, 'name')
        self._add_child_key(apn, element, 'username', 'user')
        self._add_child_key(apn, element, 'password')
        self._add_child_key(apn, element, 'dns')

    def _parse_mbpi_apns(self, carrier, tech, element, country):
        apns = []
        nwids = element.getElementsByTagName('network-id')
        dom_apns = element.getElementsByTagName('apn')
        for nwid in nwids:
            mcc = nwid.getAttribute('mcc')
            mnc = nwid.getAttribute('mnc')
            for dom_apn in dom_apns:
                apn = {
                    'carrier': carrier,
                    'technology': tech,
                    'mcc': mcc,
                    'mnc': mnc,
                    'nwid': '%s:%s' % (mcc, mnc),
                    'source': APNDB.MBPI
                }
                self._parse_mbpi_apn(apn, dom_apn)
                apns.append(apn)
        return apns

    def _parse_mbpi_provider(self, country, element):
        apns = []
        names = [x for x in element.getElementsByTagName('name') if x.parentNode == element]
        # XXX: if there's more than one name, we just take the first one. The
        # DTD allows one or more names in the provider block. I have no idea
        # what the semantics of this are.
        carrier = names[0].childNodes[0].data
        if not carrier:
            print 'Entry with no carrier code?'
        gsms = element.getElementsByTagName('gsm')
        cdmas = element.getElementsByTagName('cdma')
        for gsm in gsms:
            apns += self._parse_mbpi_apns(carrier, 'gsm', gsm, country)
        for cdma in cdmas:
            apns += self._parse_mbpi_apns(carrier, 'cdma', cdma, country)
        return apns

    def parse(self, xmlfile):
        """Loads contents from a supplied filehandle.

        The dtd for mobile-broadband-provider-info's xml files is really hairy,
        so these parsing functions are too. Oh well. See serviceproviders.2.dtd
        in the mbpi distribution for the gory details.

        """
        dom = parse(xmlfile)
        countries = dom.getElementsByTagName('country')
        for country in countries:
            providers = country.getElementsByTagName('provider')
            for provider in providers:
                self._apns += self._parse_mbpi_provider(
                    country.getAttribute('code'),
                    provider)

class MergedAPNDB(APNDB):
    """A merged APN database.

    This class overrides the add method to support incremental merging of the
    android database into the mbpi database. There are several major caveats to
    this:

    1) The logic here is extraordinarily brittle, and changes in the MBPIDB and
       AndroidAPNDB classes are likely to break it
    2) Whichever database is added first will get to decide the names of many
       common carriers, so adding the android DB before the MBPI DB may have
       catastrophic results in terms of the generated delta being very large.
    3) The merging is still imperfect and might produce duplicate carriers.

    """

    def __init__(self):
        APNDB.__init__(self)
        self._carriers = defaultdict(lambda: {'apns': [],
                                              'aliases': [],
                                              'mcc': 'none'})
        self._conflicts = 0
        self._apnmismatch = 0

    def _should_add(self, apn):
        def validapn(a):
            if 'apn' not in a:
                return False
            if 'carrier' not in a:
                return False
            if 'mcc' not in a or 'mnc' not in a:
                return False
            return True

        def matchprop(a,b,p):
            if p not in a and p not in b:
                return True
            if p in a and p not in b:
                return True # XXX: fuzzy match.
            if p not in a and p in b:
                return True
            return a[p] == b[p]

        def matchapn(a, b):
            if a['mcc'] != b['mcc']:
                return False
            if a['mnc'] != b['mnc']:
                return False
            if a['apn'] != b['apn']:
                return False
            if not matchprop(a, b, 'user'):
                return False
            if not matchprop(a, b, 'password'):
                return False
            return True

        if not validapn(apn):
            return False

        matches = [x for x in self.apns() if matchapn(apn, x)]
        if matches:
            self._conflicts += 1
            return False
        return True

    def carriers(self):
        return self._carriers

    def find_carriers(self, pred):
        return [x for x in self._carriers.values() if pred(x)]

    def _learn_carrier(self, apn):
        # When we get a new APN, we might a new carrier, or a new mcc:mnc for an
        # existing carrier.
        nwid = apn['nwid']
        carrier = apn['mcc'] + ":" + apn['carrier']
        mcc = apn['mcc']
        key = carrier

        # Let's see if we can merge carriers. We're looking for another carrier
        # with whom our set of nwids overlaps. Note that there can be at most
        # one carrier that we overlap with, nwid-wise, since if there existed
        # carriers a and b that both had our nwid, they would have been merged
        # previously.
        ournwids = [x['nwid'] for x in self._carriers[key]['apns']]
        ournwids.append(nwid)
        othercarrier = None
        for c in self._carriers:
            if key == c or mcc != self._carriers[c]['mcc']:
                continue
            theirnwids = [x['nwid'] for x in self._carriers[c]['apns']]
            if set(theirnwids) & set(ournwids):
                key = c
                break

        # Do we intersect another carrier by nwid? If so, add ourselves to their
        # list; otherwise, create a new list for just us. After this step, this
        # APN has been added to some carrier named after either:
        # 1) its own name, or
        # 2) the carrier whose nwid set already contains us.
        self._carriers[key]['apns'].append(apn)
        self._carriers[key]['mcc'] = mcc
        if carrier not in self._carriers[key]['aliases']:
            self._carriers[key]['aliases'].append(carrier)

    def _add_one(self, apn):
        # This function is responsible for actually merging new APNs into the
        # database. We want to keep all existing APNs for a particular carrier,
        # so we basically just need to avoid adding duplicate APNs.
        if self._should_add(apn):
            self._apns.append(apn)
            self._learn_carrier(apn)

    def add(self, apn):
        if type(apn) == list:
            for a in apn:
                self.add(a)
        else:
            self._add_one(apn)

    def stats(self):
        return {'conflicts': self._conflicts}

def print_carriers(db):
    carriers = db.carriers()
    res = []
    for name,c in carriers.items():
        droidapns = [x for x in c['apns'] if x['source'] == APNDB.ANDROID]
        mbpiapns = [x for x in c['apns'] if x['source'] == APNDB.MBPI]
        newapns = [x['nwid'] for x in droidapns]
        newapns = filter(lambda x: x not in [y['nwid'] for y in mbpiapns],
                         newapns)
        if newapns:
            res.append('Carrier %s: add nwids %s' % (name.encode('utf-8'),
                                                     newapns))
        newapns = [x['apn'] for x in droidapns]
        newapns = filter(lambda x: x not in [y['apn'] for y in mbpiapns],
                         newapns)
        if newapns:
            res.append('Carrier %s: add apns %s' % (name.encode('utf-8'),
                                                    newapns))
    res.sort()
    for r in res:
        print r

if len(sys.argv) < 3:
    print 'Usage: %s <android-db> <mbpi-db>' % sys.argv[0]
    sys.exit(0)

mbpi_db = MBPIDB()
mbpi_db.parse(file(sys.argv[2]))
android_db = AndroidAPNDB()
android_db.parse(file(sys.argv[1]))

result = MergedAPNDB()
result.add(mbpi_db.apns())
result.add(android_db.apns())
