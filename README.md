# Bridge

Contract bridge for Sailfish OS: you and a bot partner against two bots.

- Bidding in Standard American, two over one, Acol or Precision, chosen for each side,
  with every call explained
- Play the contract with dummy's cards when your side declares, or defend it
- Rubber bridge or Chicago scoring, a score sheet and a review of each deal
- Bots in three levels, up to one that reads the bidding and solves the last tricks exactly

Needs Sailfish OS 5.1 or newer, on aarch64 or armv7hl.

## Building

The card code shared with the other card games is the
[sailfish-cards](https://github.com/klahr/sailfish-cards) submodule, so clone with
`git clone --recursive`, or run `git submodule update --init` in an existing clone.

With the [Sailfish SDK](https://docs.sailfishos.org/Tools/Sailfish_SDK/), from the source directory:

```
sfdk -c target=SailfishOS-5.1.0.11-aarch64 build
sfdk -c target=SailfishOS-5.1.0.11-armv7hl build
```

The RPM ends up in `RPMS/`. Clean the build files between targets.

The rules, the bidding systems and the bots are plain C++ without Qt, with tests that
run on any desktop:

```
make -C tests
```

They also run when the RPM is built.

## License

GPLv3, see [LICENSE](LICENSE).
