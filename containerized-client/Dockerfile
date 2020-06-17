FROM nginx:alpine

# location of backend
ENV BACKEND_HOST=localhost
ENV BACKEND_PORT=8080

# clean default configuration and content
RUN rm -rf /usr/share/nginx/html/*
RUN rm -rf /etc/nginx/conf.d/*

# copy static assets
COPY src/ /usr/share/nginx/html

# copy template of configuration
# configurations are automatically run through envsubst to create the actual configuration
RUN mkdir -p /etc/nginx/templates
COPY default.conf.template /etc/nginx/templates