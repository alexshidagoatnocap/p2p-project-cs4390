all:
	$(MAKE) -C tracker
	$(MAKE) -C peer

setup:
	bash scripts/setup_peers.sh

clean:
	$(MAKE) -C tracker clean
	$(MAKE) -C peer clean
	bash scripts/clean_demo.sh