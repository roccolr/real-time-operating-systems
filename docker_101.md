# Docker minimal guide 
Some useful commands somehow I keep forgetting.

## Preexisting image

```bash
docker run -d --name server-web -p 8080:80 -v /opt/dati:/usr/share/nginx/html --restart unless-stopped nginx:alpine

```

- -d ~ detach;
- --name ~ explicit name instead of a random one;
- -p 8080:80 ~ publish: map host port 8080 on container port 80;
- --v /opt/dati:/usr/share/nginx/html ~ mount a host directory in the container
- --restart unless-stopped ~ self explanatory.

## Build an image
First thing you should do is creating a ```Dockerfile``` in your application directory on the host.

```bash
FROM python:3.11-slim
WORKDIR /app
COPY app.py .
CMD ["python", "app.py"]
```

then you build the image (it will automatically check for the Dockerfile).

```bash
docker build -t mia-app:v1.0 .
```

- -t mia-app:v1.0 . ~ assing a name (tag) to the image. The final point means the build context is the working directory. 

then you run it:

```bash
docker run -d --name test-python mia-app:v1.0
```


## Monitor your container 
```bash
docker ps

docker logs -f --tail 50 server-web

docker stats # htop style

docker inspect server-web # low level variables
```

## Useful cmds
```bash
docker stop server-web

docker kill server-web # hard stop

docker start server-web
docker start -a server-web # log on the shell

docker rm server-web # remove the container from the disk
docker rm -f server-web # remove even if it is alive 

docker rmi nginx:alpine # remove a static useless image

docker system prune # no mercy for useless stuff
```

# Compose 
U may need an application that requires communication with other containers (web server and db). We can use declarative yml file (docker-compose.yml):

```yaml
services:
  web:
    image: nginx:alpine
    ports:
      - "8080:80"
    depends_on:
      - redis

  redis:
    image: redis:7-alpine
    volumes:
      - redis-dati:/data

volumes:
  redis-dati:
```

```bash 
docker compose up -d

# ...

docker compose down
```