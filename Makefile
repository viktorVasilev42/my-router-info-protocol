up:
	docker compose up --build -d

down:
	docker compose down --rmi all

logs:
	docker compose logs -f

rmi:
	docker images -q | xargs docker rmi

grapher:
	docker compose up --build topology-grapher -d
