#
# Makefile pour le projet de fin de semestre 
#
# GOKHAN KABAR 
#
# Constantes pour la compilation des programmes
#

export CC = gcc
export LD = gcc
export CLIB = ar cq
export CFLAGS = -Wall
export CDEBUG = -g -DDEBUG

#
# Constantes liées au projet
#

DIRS = Serveur Client 

#
# La cible générale
#
all: $(patsubst %, _dir_%, $(DIRS))

DEBUG: CFLAGS += $(CDEBUG)
DEBUG: all

d: CFLAGS += $(CDEBUG)
d: all

$(patsubst %,_dir_%,$(DIRS)):
	cd $(patsubst _dir_%,%,$@) && make

#
# La cible de nettoyage
#

clean: $(patsubst %, _clean_%, $(DIRS))

$(patsubst %,_clean_%,$(DIRS)):
	cd $(patsubst _clean_%,%,$@) && make clean
