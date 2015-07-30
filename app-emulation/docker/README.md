## Tips and Tricks

To hack on docker and develop a patch do these steps:

```
git clone https://github.com/docker/docker
cd docker
ln -s ../../../../ vendor/src/github.com/docker/docker
./hack/make.sh dynbinary
```

Then add some symlinks:

```
ln -s $PWD/bundles/VERSION-OF-DOCKER-dev/dynbinary/docker /usr/local/bin/docker
ln -s $PWD/bundles/VERSION-OF-DOCKER-dev/dynbinary/dockerinit /usr/local/bin/dockerinit
```
