# This should only be run on very few machines anyway,
# so this isn't included in the CMakeLists.txt

LOCALES=$(shell cat po/LINGUAS)
LOCALE_MOS=$(addprefix po/,$(addsuffix .mo,$(LOCALES)))
MSGFMT_PATH=msgfmt

all: translate

po/%.mo: po/%.po
	${MSGFMT_PATH} -cf -o $@ $<

po/tdlib-purple.metainfo.xml.pot: tdlib-purple.metainfo.untranslated.xml as-metainfo.its
	itstool -i as-metainfo.its -o $@ $<

# CAREFUL WHEN EDITING!
# "-j raw.xml *.mo" must be the last options!
tdlib-purple.metainfo.xml: as-metainfo.its tdlib-purple.metainfo.untranslated.xml $(LOCALE_MOS) po/LINGUAS
	itstool -i as-metainfo.its -o $@ \
	        -j tdlib-purple.metainfo.untranslated.xml $(LOCALE_MOS)

.PHONY: potfile
potfile: po/tdlib-purple.metainfo.xml.pot

.PHONY: translate
translate: tdlib-purple.metainfo.xml

.PHONY: clean
clean:
	rm -rf po/*.mo
