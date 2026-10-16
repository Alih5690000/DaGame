em++ main.cpp -o index.html \
  -s USE_SDL=2 \
  -s USE_SDL_TTF=2 \
  -s USE_SDL_IMAGE=2 \
  -s SDL2_IMAGE_FORMATS='["png","jpg"]' \
  -s USE_SDL_MIXER=2 \
  -s SDL2_MIXER_FORMATS='["ogg","mp3","wav"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file assets \
  -g3 \
  -sASSERTIONS=2 \
  -sSAFE_HEAP=1 \
  -sSTACK_OVERFLOW_CHECK=2 \
  -sNO_DISABLE_EXCEPTION_CATCHING \
  && python -m http.server 8000