ifeq ($(strip $(FROSK)),)
  # Building for the environment we are in
  include Makefile.host
else
  # Building for Frosk image
  include $(CURD)/Makefile.frosk
endif
